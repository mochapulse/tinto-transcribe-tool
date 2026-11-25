// Passive shared-mode input and read-only system audio capture via miniaudio.

#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio.h"

#include <array>
#include <cctype>
#include <chrono>
#include <iostream>
#include <optional>
#include <string>
#include <utility>

#ifdef _WIN32
#include <cfgmgr32.h>
#include <cwchar>
#endif

#include "tinto/capture_factory.hpp"

namespace tinto {
namespace {

using Clock = std::chrono::steady_clock;
constexpr auto kDeviceCheckInterval = std::chrono::seconds(2);

bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
  if (needle.size() > haystack.size()) return false;
  for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      if (std::tolower(static_cast<unsigned char>(haystack[i + j])) !=
          std::tolower(static_cast<unsigned char>(needle[j]))) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

bool IsSafeAutomaticInput(const ma_context& context, const ma_device_id& id) {
#ifdef _WIN32
  (void)context;
  // Core Audio endpoints are software nodes. Check their physical ancestry
  // before opening one so Bluetooth microphones cannot switch audio profiles.
  std::wstring instance = L"SWD\\MMDEVAPI\\";
  instance += id.wasapi;
  DEVINST node = 0;
  if (CM_Locate_DevNodeW(&node, instance.data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS) {
    return false;
  }

  bool known_transport = false;
  for (int depth = 0; depth < 32; ++depth) {
    wchar_t name[MAX_DEVICE_ID_LEN]{};
    if (CM_Get_Device_IDW(node, name, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS) return false;
    if (_wcsnicmp(name, L"BTH", 3) == 0 || _wcsnicmp(name, L"BLUETOOTH", 9) == 0) {
      return false;
    }
    for (const auto* prefix : {L"HDAUDIO\\", L"INTELAUDIO\\", L"USB\\", L"PCI\\"}) {
      if (_wcsnicmp(name, prefix, std::wcslen(prefix)) == 0) known_transport = true;
    }
    DEVINST parent = 0;
    const auto result = CM_Get_Parent(&parent, node, 0);
    if (result == CR_NO_SUCH_DEVNODE) return known_transport;
    if (result != CR_SUCCESS || parent == node) return false;
    node = parent;
  }
  return false;
#else
  // A Pulse/PipeWire ALSA source is not a Bluetooth profile or sink monitor.
  return context.backend == ma_backend_pulseaudio &&
         std::string(id.pulse).rfind("alsa_input.", 0) == 0;
#endif
}

class MiniaudioCapture final : public ICapture {
 public:
  ~MiniaudioCapture() override { Stop(); }

  bool Start(CaptureSelection selection, const std::string& device_substring,
             Callback cb) override {
    Stop();
    callback_ = std::move(cb);
    if (!callback_) return false;

    input_name_ = device_substring;
    const bool allow_partial = selection == CaptureSelection::kDefault && input_name_.empty();
    if (selection == CaptureSelection::kDefault) {
      selection = input_name_.empty() ? CaptureSelection::kBoth : CaptureSelection::kInput;
    }
    want_input_ = selection != CaptureSelection::kSystem;
    want_system_ = selection != CaptureSelection::kInput;

#ifdef _WIN32
    constexpr ma_backend kBackends[] = {ma_backend_wasapi};
#else
    constexpr ma_backend kBackends[] = {ma_backend_pulseaudio, ma_backend_alsa};
#endif
    if (ma_context_init(kBackends, sizeof(kBackends) / sizeof(kBackends[0]),
                        nullptr, &context_) != MA_SUCCESS) return false;
    context_initialized_ = true;

    UpdateDevices(true);
    next_check_ = Clock::now() + kDeviceCheckInterval;
    const bool input = devices_[0].active;
    const bool system = devices_[1].active;
    if (!(input || system) ||
        (!allow_partial && ((want_input_ && !input) || (want_system_ && !system)))) {
      Stop();
      return false;
    }
    ReportSources();
    return true;
  }

  void Refresh() override {
    if (!context_initialized_ || Clock::now() < next_check_) return;
    next_check_ = Clock::now() + kDeviceCheckInterval;
    if (UpdateDevices(false)) ReportSources();
  }

  void Stop() override {
    for (auto& device : devices_) device.Stop();
    if (context_initialized_) {
      ma_context_uninit(&context_);
      context_initialized_ = false;
    }
    callback_ = nullptr;
  }

 private:
  struct Device {
    ma_device handle{};
    ma_device_id id{};
    MiniaudioCapture* owner = nullptr;
    AudioSource source = AudioSource::kInput;
    bool active = false;

    void Stop() {
      if (!active) return;
      ma_device_uninit(&handle);  // Waits for any in-flight callback.
      active = false;
    }
  };

  bool UpdateDevices(bool initial) {
    ma_device_info* playback = nullptr;
    ma_device_info* capture = nullptr;
    ma_uint32 playback_count = 0;
    ma_uint32 capture_count = 0;
    if (ma_context_get_devices(&context_, &playback, &playback_count, &capture,
                               &capture_count) != MA_SUCCESS) return false;

    std::optional<ma_device_id> input;
    std::optional<ma_device_id> system;
    if (want_input_) {
      for (ma_uint32 i = 0; i < capture_count; ++i) {
        const auto& candidate = capture[i];
        if (input_name_.empty() ? !candidate.isDefault
                               : !ContainsIgnoreCase(candidate.name, input_name_)) continue;
        const bool safe = IsSafeAutomaticInput(context_, candidate.id);
        if (safe || !input_name_.empty()) input = candidate.id;
        const auto& current = devices_[0];
        const bool different = input && (!current.active ||
            !ma_device_id_equal(&current.id, &*input));
        if (!safe && (initial || different || (!input && current.active))) {
          std::cerr << (input ? "capture: explicitly selected input may be Bluetooth; "
                               "its use can change playback quality\n"
                             : "capture: skipped default input with Bluetooth or "
                               "unverified transport; use --device to opt in\n");
        }
        break;
      }
    }
    if (want_system_) {
      for (ma_uint32 i = 0; i < playback_count; ++i) {
        if (!playback[i].isDefault) continue;
#ifdef _WIN32
        system = playback[i].id;
#else
        if (context_.backend == ma_backend_pulseaudio) {
          const std::string monitor = std::string(playback[i].id.pulse) + ".monitor";
          for (ma_uint32 j = 0; j < capture_count; ++j) {
            if (monitor == capture[j].id.pulse) {
              system = capture[j].id;
              break;
            }
          }
        }
#endif
        break;
      }
    }

    const bool input_changed = UpdateDevice(devices_[0], input, AudioSource::kInput);
    const bool system_changed = UpdateDevice(devices_[1], system, AudioSource::kSystem);
    return input_changed || system_changed;
  }

  bool UpdateDevice(Device& device, const std::optional<ma_device_id>& id,
                    AudioSource source) {
    if (device.active && id && ma_device_id_equal(&device.id, &*id) &&
        ma_device_is_started(&device.handle)) return false;

    const bool was_active = device.active;
    device.Stop();
    if (!id) return was_active;

    auto type = ma_device_type_capture;
#ifdef _WIN32
    if (source == AudioSource::kSystem) type = ma_device_type_loopback;
#endif
    ma_device_config config = ma_device_config_init(type);
    config.capture.pDeviceID = &*id;
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.capture.shareMode = ma_share_mode_shared;
    config.sampleRate = kCaptureSampleRate;
    config.dataCallback = &MiniaudioCapture::OnData;
    config.pUserData = &device;
    device.owner = this;
    device.source = source;
    device.id = *id;
    if (ma_device_init(&context_, &config, &device.handle) != MA_SUCCESS) return was_active;

    device.active = true;
    if (ma_device_start(&device.handle) == MA_SUCCESS) return true;
    device.Stop();
    return was_active;
  }

  static void OnData(ma_device* device, void*, const void* input, ma_uint32 frame_count) {
    const auto* slot = static_cast<const Device*>(device->pUserData);
    if (!input || !frame_count) return;
    slot->owner->callback_({static_cast<const float*>(input), frame_count,
                           kCaptureSampleRate, 1, slot->source, Clock::now()});
  }

  void ReportSources() const {
    const bool input = devices_[0].active;
    const bool system = devices_[1].active;
    std::cout << "capture: active sources: "
              << (input && system ? "input and system" : input ? "input" :
                  system ? "system" : "none") << std::endl;
  }

  ma_context context_{};
  std::array<Device, 2> devices_{};
  bool context_initialized_ = false;
  bool want_input_ = false;
  bool want_system_ = false;
  std::string input_name_;
  Clock::time_point next_check_{};
  Callback callback_;
};

}  // namespace

std::unique_ptr<ICapture> MakeCapture() {
  return std::make_unique<MiniaudioCapture>();
}

}  // namespace tinto
