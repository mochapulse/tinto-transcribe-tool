#include "tinto/pipeline.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>

#include "tinto/capture_factory.hpp"
#include "tinto/paths.hpp"

#ifdef TINTO_WITH_WHISPER
#include "tinto/gate_factory.hpp"
#include "tinto/transcriber_factory.hpp"
#endif

namespace tinto {
namespace {

namespace fs = std::filesystem;

std::string TimestampFilename() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  const std::tm* local = std::localtime(&time);
  std::ostringstream out;
  const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count() % 1000;
  out << std::put_time(local, "%Y%m%d-%H%M%S") << '-'
      << std::setfill('0') << std::setw(3) << milliseconds;
  return out.str();
}

std::string FormatSeconds(double sec) {
  const auto millis = std::llround(sec * 1000);
  const auto total = millis / 1000;
  const auto h = total / 3600;
  const auto m = (total % 3600) / 60;
  const auto s = total % 60;
  std::ostringstream out;
  out << std::setfill('0') << std::setw(2) << h << ":"
      << std::setfill('0') << std::setw(2) << m << ":"
      << std::setfill('0') << std::setw(2) << s << '.' << std::setw(3) << millis % 1000;
  return out.str();
}

std::string ModelNameFromPath(const std::string& path) {
  return fs::path(path).stem().string();
}

}  // namespace

Pipeline::Pipeline(Config cfg, ModelPaths paths)
    : cfg_(std::move(cfg)), model_paths_(std::move(paths)), windows_(cfg_.window_sec) {}

Pipeline::~Pipeline() { Finish([] { return true; }); }

bool Pipeline::Start() {
  const std::string ts = TimestampFilename();

  capture_ = MakeCapture();
  if (!capture_) {
    std::cerr << "pipeline: failed to create capture device\n";
    return false;
  }

  const bool needs_transcription = (cfg_.mode == Mode::kRecordTranscribe ||
                                    cfg_.mode == Mode::kTranscribeOnly);

#ifdef TINTO_WITH_WHISPER
  if (needs_transcription) {
    if (model_paths_.vad_model.empty() || !fs::exists(model_paths_.vad_model)) {
      std::cerr << "pipeline: VAD model not found: " << model_paths_.vad_model
                << "\n  hint: download models into " << paths::models_dir().string()
                << "/\n";
      return false;
    }
    gate_ = MakeWhisperVadGate(model_paths_.vad_model);

    if (model_paths_.asr_model.empty() || !fs::exists(model_paths_.asr_model)) {
      std::cerr << "pipeline: ASR model not found: " << model_paths_.asr_model
                << "\n  hint: download models into " << paths::models_dir().string()
                << "/\n";
      return false;
    }
    const int threads = cfg_.threads > 0
                            ? cfg_.threads
                            : static_cast<int>(std::thread::hardware_concurrency());
    transcriber_ = MakeWhisperTranscriber(model_paths_.asr_model, cfg_.lang, threads);
  }
#else
  if (needs_transcription) {
    std::cerr << "pipeline: built without whisper — transcription unavailable\n";
    return false;
  }
#endif

  mixer_.Reset(AudioMixer::Clock::now());
  accept_audio_.store(true, std::memory_order_relaxed);
  auto cb = [this](const AudioFrame& frame) {
    if (accept_audio_.load(std::memory_order_relaxed)) mixer_.Push(frame);
  };
  if (!capture_->Start(cfg_.source, cfg_.device, std::move(cb))) {
    std::cerr << "pipeline: failed to start requested capture sources"
              << (cfg_.device.empty() ? "" : " (device: " + cfg_.device + ")")
              << "\n";
    return false;
  }

  const bool needs_audio = (cfg_.mode == Mode::kRecordOnly ||
                            cfg_.mode == Mode::kRecordTranscribe);

  if (needs_audio) {
    const fs::path rec_dir = cfg_.out.empty()
                                 ? paths::recordings_dir()
                                 : fs::path(cfg_.out);
    fs::create_directories(rec_dir);
    wav_path_ = (rec_dir / (ts + (cfg_.diarize ? ".aligned.wav" : ".wav"))).string();
    if (cfg_.diarize) wav_staging_path_ = (rec_dir / (ts + ".incomplete.wav")).string();
    if (!wav_writer_.Open(wav_staging_path_.empty() ? wav_path_ : wav_staging_path_)) {
      std::cerr << "pipeline: cannot create " << wav_path_ << "\n";
      return false;
    }
  }

  if (needs_transcription) {
    const fs::path tx_dir = cfg_.out.empty()
                                ? paths::transcripts_dir()
                                : fs::path(cfg_.out);
    fs::create_directories(tx_dir);
    md_path_ = (tx_dir / (ts + ".md")).string();
    md_file_.open(md_path_, std::ios::trunc);
    if (!md_file_) {
      std::cerr << "pipeline: cannot create " << md_path_ << "\n";
      return false;
    }
    WriteTranscriptHeader();
  }

  started_ = true;
  std::cout << "pipeline: started"
            << " mode=" << ToString(cfg_.mode)
            << " lang=" << cfg_.lang;
  if (needs_transcription) {
    std::cout << " asr=" << ModelNameFromPath(model_paths_.asr_model);
  }
  if (needs_audio) {
    std::cout << " wav=" << wav_path_;
  }
  if (needs_transcription) {
    std::cout << " md=" << md_path_;
  }
  std::cout << "\n";
  return true;
}

void Pipeline::Run(const CancelTranscription& stop_requested,
                   const CancelTranscription& cancel_requested) {
  constexpr auto kPollInterval = std::chrono::milliseconds(200);
  // Leave headroom below the Windows stop client's wait timeout.
  constexpr auto kShutdownBudget = std::chrono::seconds(25);
  std::optional<AudioMixer::Clock::time_point> deadline;
  const CancelTranscription should_cancel = [&] {
    if (!deadline && stop_requested()) {
      accept_audio_.store(false, std::memory_order_relaxed);
      deadline = AudioMixer::Clock::now() + kShutdownBudget;
    }
    return cancel_requested() || (deadline && AudioMixer::Clock::now() >= *deadline);
  };

  while (!stop_requested()) {
    capture_->Refresh();
    ProcessBuffer(false, should_cancel);
    if (stop_requested()) break;
    std::this_thread::sleep_for(kPollInterval);
  }
  should_cancel();  // Start the deadline even when we stopped between batches.
  std::cout << "record: finalizing captured audio" << std::endl;
  Finish(should_cancel);
}

void Pipeline::Finish(const CancelTranscription& should_cancel) {
  if (finished_) return;
  finished_ = true;

  accept_audio_.store(false, std::memory_order_relaxed);
  if (capture_) {
    capture_->Stop();
    capture_.reset();
  }

  if (!started_) return;
  ProcessBuffer(true, should_cancel);

  wav_writer_.Close();
  if (!wav_staging_path_.empty()) {
    std::error_code error;
    if (aligned_wav_ok_) fs::rename(wav_staging_path_, wav_path_, error);
    if (!aligned_wav_ok_ || error) {
      std::cerr << "pipeline: could not finalize aligned WAV; partial audio: "
                << wav_staging_path_ << "\n";
      wav_path_.clear();
    }
  }

  if (md_file_.is_open()) {
    md_file_ << "\n---\n*Recording ended. " << segments_written_
             << " segment(s) transcribed.*\n";
    if (transcription_interrupted_) {
      md_file_ << "\n*Shutdown left some audio untranscribed.*\n";
    }
    md_file_.close();
  }

  if (!wav_path_.empty()) {
    std::cout << "pipeline: audio saved to " << wav_path_ << "\n";
  }
  if (!md_path_.empty()) {
    std::cout << "pipeline: transcript saved to " << md_path_ << "\n";
  }

  if (cfg_.mode == Mode::kTranscribeOnly && !wav_path_.empty()) {
    std::error_code ec;
    fs::remove(wav_path_, ec);
  }

  gate_.reset();
  transcriber_.reset();
}

void Pipeline::ProcessBuffer(bool final, const CancelTranscription& should_cancel) {
  const auto audio = mixer_.Drain(AudioMixer::Clock::now(), final);
  if (cfg_.mode == Mode::kRecordOnly) {
    if (!audio.empty()) wav_writer_.Write(audio.data(), audio.size());
    return;
  }
  if (!gate_ || (!final && audio.empty())) return;
  if (!audio.empty()) gate_->Feed(audio.data(), audio.size());
  auto segments = gate_->Drain(final);

  if (cfg_.mode == Mode::kRecordTranscribe) {
    for (const auto& seg : segments) {
      if (!cfg_.diarize) {
        wav_writer_.Write(seg.samples.data(), seg.samples.size());
      } else if (aligned_wav_ok_) {
        const auto start = static_cast<std::uint64_t>(std::llround(seg.start_sec * kCaptureSampleRate));
        aligned_wav_ok_ = wav_writer_.WriteAligned(start, seg.samples.data(), seg.samples.size());
      }
    }
  }

  if (!transcriber_) return;
  for (const auto& window : windows_.TakeReady(segments, gate_->StableThrough(), final)) {
    if (transcription_interrupted_ || should_cancel()) {
      transcription_interrupted_ = true;
      break;
    }
    Transcript tx = transcriber_->Transcribe(
        window.samples.data(), window.samples.size(), kCaptureSampleRate, should_cancel);
    AppendTranscript(window, tx);
    transcription_interrupted_ = tx.interrupted;
  }
}

void Pipeline::WriteTranscriptHeader() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  const std::tm* local = std::localtime(&time);

  md_file_ << "# Meeting Transcript\n\n"
           << "- **Date:** ";
  char buf[64];
  std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", local);
  md_file_ << buf << "\n"
           << "- **Mode:** " << ToString(cfg_.mode) << "\n"
           << "- **Language:** " << cfg_.lang << "\n";
  if (transcriber_) {
    md_file_ << "- **Model:** " << ModelNameFromPath(model_paths_.asr_model) << "\n";
  }
  md_file_ << "\n---\n\n";
  md_file_.flush();
}

void Pipeline::AppendTranscript(const SpeechSegment& window,
                                const Transcript& tx) {
  if (tx.text.empty() || !md_file_.is_open()) return;

  const auto append = [this](double start, double end, const std::string& text) {
    md_file_ << "**[" << FormatSeconds(start) << " — " << FormatSeconds(end) << "]**\n"
             << text << "\n\n";
    ++segments_written_;
  };
  const auto before = segments_written_;
  for (const auto& token : tx.tokens) {
    const auto start = window.start_sec + std::max(0.0, token.start_sec);
    const auto end = std::min(window.end_sec, window.start_sec + token.end_sec);
    if (!token.text.empty() && end > start) append(start, end, token.text);
  }
  if (segments_written_ == before) append(window.start_sec, window.end_sec, tx.text);
  md_file_.flush();
}

}  // namespace tinto
