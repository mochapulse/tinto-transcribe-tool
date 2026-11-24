// miniaudio-backed implementation of tinto::ICapture.
//
// Passive sniffer: capture devices are always opened in SHARED mode and never
// in exclusive mode, so other applications keep using the device.

#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "tinto/capture.hpp"
#include "tinto/capture_factory.hpp"

namespace tinto {
namespace {

constexpr std::uint32_t kSampleRate = 16000;  // whisper's target rate
constexpr std::uint32_t kChannels = 1;        // mono

// Case-insensitive substring test (ASCII only; portable, no strcasestr).
bool ContainsIgnoreCase(const std::string& haystack, const std::string& needle) {
  if (needle.empty()) return true;
  if (needle.size() > haystack.size()) return false;
  for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
    bool match = true;
    for (std::size_t j = 0; j < needle.size(); ++j) {
      const auto a = std::tolower(static_cast<unsigned char>(haystack[i + j]));
      const auto b = std::tolower(static_cast<unsigned char>(needle[j]));
      if (a != b) {
        match = false;
        break;
      }
    }
    if (match) return true;
  }
  return false;
}

class MiniaudioCapture final : public ICapture {
 public:
  MiniaudioCapture() = default;

  ~MiniaudioCapture() override { Stop(); }

  MiniaudioCapture(const MiniaudioCapture&) = delete;
  MiniaudioCapture& operator=(const MiniaudioCapture&) = delete;

  bool Start(const std::string& device_substring, Callback cb) override {
    Stop();  // restart cleanly if already running

    callback_ = std::move(cb);
    if (!callback_) return false;

    if (ma_context_init(nullptr, 0, nullptr, &context_) != MA_SUCCESS) {
      return false;
    }
    context_initialized_ = true;

    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = kChannels;
    config.capture.shareMode = ma_share_mode_shared;  // never exclusive
    config.sampleRate = kSampleRate;
    config.dataCallback = &MiniaudioCapture::OnData;
    config.pUserData = this;

    ma_device_id chosen_id{};
    if (!device_substring.empty()) {
      if (!FindCaptureDevice(device_substring, &chosen_id)) {
        Stop();
        return false;
      }
      config.capture.pDeviceID = &chosen_id;
    }

    if (ma_device_init(&context_, &config, &device_) != MA_SUCCESS) {
      Stop();
      return false;
    }
    device_initialized_ = true;

    if (ma_device_start(&device_) != MA_SUCCESS) {
      Stop();
      return false;
    }
    return true;
  }

  void Stop() override {
    if (device_initialized_) {
      ma_device_stop(&device_);  // waits for any in-flight callback to return
      ma_device_uninit(&device_);
      device_initialized_ = false;
    }
    if (context_initialized_) {
      ma_context_uninit(&context_);
      context_initialized_ = false;
    }
    callback_ = nullptr;
  }

 private:
  // Looks up the first capture device whose name contains `substring`
  // (case-insensitive). Requires an initialized context.
  bool FindCaptureDevice(const std::string& substring, ma_device_id* out_id) {
    ma_device_info* playback = nullptr;
    ma_device_info* capture = nullptr;
    ma_uint32 playback_count = 0;
    ma_uint32 capture_count = 0;
    if (ma_context_get_devices(&context_, &playback, &playback_count, &capture,
                               &capture_count) != MA_SUCCESS) {
      return false;
    }
    for (ma_uint32 i = 0; i < capture_count; ++i) {
      if (ContainsIgnoreCase(capture[i].name, substring)) {
        *out_id = capture[i].id;
        return true;
      }
    }
    return false;
  }

  static void OnData(ma_device* device, void* /*output*/, const void* input,
                     ma_uint32 frame_count) {
    auto* self = static_cast<MiniaudioCapture*>(device->pUserData);
    if (!self || !self->callback_ || !input) return;

    AudioFrame frame{};
    frame.samples = static_cast<const float*>(input);
    frame.count = frame_count;
    frame.sample_rate = kSampleRate;
    frame.channels = kChannels;
    self->callback_(frame);
  }

  ma_context context_{};
  ma_device device_{};
  bool context_initialized_ = false;
  bool device_initialized_ = false;
  Callback callback_;
};

}  // namespace

std::unique_ptr<ICapture> MakeCapture() {
  return std::make_unique<MiniaudioCapture>();
}

}  // namespace tinto
