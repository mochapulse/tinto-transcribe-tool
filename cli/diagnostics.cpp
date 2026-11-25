#include "diagnostics.hpp"

#include <exception>
#include <iostream>

#include "tinto/capture_factory.hpp"
#include "tinto/daemon.hpp"
#include "tinto/model_install.hpp"
#include "tinto/paths.hpp"
#include "tinto/profile.hpp"
#include "tinto/version.hpp"

int CmdDiagnostics(bool detailed) {
  const char* command = detailed ? "doctor" : "health";
  try {
    const auto assets = tinto::RequiredModels();
    const auto profile = tinto::ReadProfile();
    std::size_t ready = 0;
    if (detailed) {
      std::cout << "tinto " << tinto::kVersion << "\n"
                << "  data directory: " << tinto::paths::home() << "\n"
                << "  profile: " << tinto::paths::profile() << "\n";
      if (profile) {
        std::cout << "  system: " << profile->os << " / " << profile->distro
                  << ", " << profile->cores_physical << " cores, " << profile->ram_gb << " GB RAM\n";
      }
      std::cout << "  model downloader: "
                << (tinto::ModelDownloaderAvailable() ? "curl available" : "curl missing") << "\n";
#ifdef TINTO_WITH_WHISPER
      std::cout << "  transcription: Whisper + Silero\n";
#else
      std::cout << "  transcription: not built in\n";
#endif
#ifdef TINTO_WITH_DIARIZATION
      std::cout << "  speaker diarization: sherpa-onnx (local CPU)\n";
#else
      std::cout << "  speaker diarization: not built in\n";
#endif
    }
    for (const auto& asset : assets) {
      const auto state = tinto::InspectModel(tinto::paths::models_dir() / asset.name, asset, detailed);
      if (state == tinto::ModelState::kReady) ++ready;
      if (detailed || state != tinto::ModelState::kReady) {
        std::cout << "  " << asset.name << ": " << tinto::ToString(state) << "\n";
      }
    }
    const auto capture = tinto::ProbeCapture();
    const auto pid = tinto::ReadPid();
    const bool ok = profile.has_value() && ready == assets.size();
    std::cout << command << ": " << (ok ? "ready" : "setup incomplete")
              << " (" << ready << '/' << assets.size() << " models, profile "
              << (profile ? "ready" : "missing or invalid") << ")\n";
    if (capture.enumerated && (capture.inputs || capture.outputs)) {
      std::cout << "  audio: " << capture.backend << ", " << capture.inputs
                << " input(s), " << capture.outputs << " output(s)\n";
    } else {
      std::cout << "  audio: no endpoints available in this session\n";
    }
    std::cout << "  worker: " << (tinto::IsProcessAlive(pid) ? "running" : "idle") << "\n";
    if (!ok) std::cout << "  run 'tinto init' to set up or repair the model cache\n";
    if (!capture.enumerated && detailed) {
      std::cout << "  check the Windows Audio service or the Linux PulseAudio/PipeWire session\n";
    }
    return ok ? 0 : 1;
  } catch (const std::exception& error) {
    std::cerr << command << ": " << error.what() << "\n";
    return 1;
  }
}
