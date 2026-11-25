#include "tinto/transcriber_factory.hpp"

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <utility>

#include <whisper.h>

namespace tinto {
namespace {

// whisper.cpp reports segment timestamps in centiseconds (10 ms units).
constexpr double kWhisperTickSeconds = 0.01;

struct Cancellation {
  const CancelTranscription& requested;
  std::atomic<bool> interrupted{false};

  static bool Check(void* data) {
    auto& self = *static_cast<Cancellation*>(data);
    if (!self.requested || !self.requested()) return false;
    self.interrupted.store(true, std::memory_order_relaxed);
    return true;
  }
};

class WhisperTranscriber final : public ITranscriber {
 public:
  WhisperTranscriber(const std::string& model_path, std::string lang, int threads)
      : lang_(std::move(lang)), threads_(ResolveThreads(threads)) {
    ctx_ = whisper_init_from_file_with_params(model_path.c_str(),
                                              whisper_context_default_params());
  }

  ~WhisperTranscriber() override {
    if (ctx_ != nullptr) {
      whisper_free(ctx_);
    }
  }

  WhisperTranscriber(const WhisperTranscriber&) = delete;
  WhisperTranscriber& operator=(const WhisperTranscriber&) = delete;

  Transcript Transcribe(const float* samples, std::size_t count,
                        std::uint32_t sample_rate,
                        const CancelTranscription& should_cancel) override {
    // Input is assumed to be mono at 16 kHz; no resampling happens here.
    (void)sample_rate;

    Transcript result;
    if (ctx_ == nullptr || samples == nullptr || count == 0) {
      return result;
    }
    Cancellation cancellation{should_cancel};
    if (Cancellation::Check(&cancellation)) {
      result.interrupted = true;
      return result;
    }

    whisper_full_params params =
        whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.language = lang_.c_str();
    params.n_threads = threads_;
    params.print_progress = false;
    params.print_realtime = false;
    params.encoder_begin_callback = [](whisper_context*, whisper_state*, void* data) {
      return !Cancellation::Check(data);
    };
    params.encoder_begin_callback_user_data = &cancellation;
    params.abort_callback = &Cancellation::Check;
    params.abort_callback_user_data = &cancellation;

    const int status = whisper_full(ctx_, params, samples, static_cast<int>(count));
    result.interrupted = cancellation.interrupted.load(std::memory_order_relaxed);
    if (status != 0 && !result.interrupted) return result;

    // On cancellation Whisper can still have complete segments from this call.
    const int n_segments = whisper_full_n_segments(ctx_);
    for (int i = 0; i < n_segments; ++i) {
      const char* segment_text = whisper_full_get_segment_text(ctx_, i);
      if (segment_text == nullptr) {
        continue;
      }
      std::string text = segment_text;
      result.text += text;

      Token token;
      token.text = std::move(text);
      token.start_sec = static_cast<double>(whisper_full_get_segment_t0(ctx_, i)) *
                        kWhisperTickSeconds;
      token.end_sec = static_cast<double>(whisper_full_get_segment_t1(ctx_, i)) *
                      kWhisperTickSeconds;
      result.tokens.push_back(std::move(token));
    }
    return result;
  }

 private:
  static int ResolveThreads(int requested) {
    if (requested > 0) {
      return requested;
    }
    const unsigned int hw = std::thread::hardware_concurrency();
    return static_cast<int>(std::max(1u, hw));
  }

  whisper_context* ctx_ = nullptr;
  std::string lang_;
  int threads_;
};

}  // namespace

std::unique_ptr<ITranscriber> MakeWhisperTranscriber(const std::string& model_path,
                                                     const std::string& lang,
                                                     int threads) {
  return std::make_unique<WhisperTranscriber>(model_path, lang, threads);
}

}  // namespace tinto
