#include "tinto/pipeline.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
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
  out << std::put_time(local, "%Y%m%d-%H%M%S");
  return out.str();
}

std::string FormatSeconds(double sec) {
  const int total = static_cast<int>(sec);
  const int h = total / 3600;
  const int m = (total % 3600) / 60;
  const int s = total % 60;
  std::ostringstream out;
  out << std::setfill('0') << std::setw(2) << h << ":"
      << std::setfill('0') << std::setw(2) << m << ":"
      << std::setfill('0') << std::setw(2) << s;
  return out.str();
}

std::string ModelNameFromPath(const std::string& path) {
  return fs::path(path).stem().string();
}

}  // namespace

Pipeline::Pipeline(Config cfg, ModelPaths paths)
    : cfg_(std::move(cfg)), model_paths_(std::move(paths)) {}

Pipeline::~Pipeline() { Finish(); }

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

  const bool needs_audio = (cfg_.mode == Mode::kRecordOnly ||
                            cfg_.mode == Mode::kRecordTranscribe);

  if (needs_audio) {
    const fs::path rec_dir = cfg_.out.empty()
                                 ? paths::recordings_dir()
                                 : fs::path(cfg_.out);
    fs::create_directories(rec_dir);
    wav_path_ = (rec_dir / (ts + ".wav")).string();
    if (!wav_writer_.Open(wav_path_)) {
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

  auto cb = [this](const AudioFrame& frame) { OnAudio(frame); };
  if (!capture_->Start(cfg_.device, std::move(cb))) {
    std::cerr << "pipeline: failed to start capture"
              << (cfg_.device.empty() ? "" : " (device: " + cfg_.device + ")")
              << "\n";
    return false;
  }

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

void Pipeline::Run(std::atomic<bool>& stop_flag) {
  constexpr auto kPollInterval = std::chrono::milliseconds(200);

  while (!stop_flag.load(std::memory_order_relaxed)) {
    ProcessBuffer();
    std::this_thread::sleep_for(kPollInterval);
  }

  ProcessBuffer();
}

void Pipeline::Finish() {
  if (finished_) return;
  finished_ = true;

  if (capture_) {
    capture_->Stop();
    capture_.reset();
  }

  ProcessBuffer();

  wav_writer_.Close();

  if (md_file_.is_open()) {
    md_file_ << "\n---\n*Recording ended. " << segments_written_
             << " segment(s) transcribed.*\n";
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

void Pipeline::OnAudio(const AudioFrame& frame) {
  if (frame.samples == nullptr || frame.count == 0) return;

  if (cfg_.mode == Mode::kRecordOnly) {
    wav_writer_.Write(frame.samples, frame.count);
    return;
  }

  std::lock_guard<std::mutex> lock(audio_mutex_);
  audio_buf_.insert(audio_buf_.end(), frame.samples,
                    frame.samples + frame.count);
}

void Pipeline::ProcessBuffer() {
  if (!gate_) return;

  std::vector<float> local;
  {
    std::lock_guard<std::mutex> lock(audio_mutex_);
    if (audio_buf_.empty()) return;
    local.swap(audio_buf_);
  }

  gate_->Feed(local.data(), local.size());
  auto segments = gate_->Drain();

  for (auto& seg : segments) {
    if (cfg_.mode == Mode::kRecordTranscribe) {
      wav_writer_.Write(seg.samples.data(), seg.samples.size());
    }

    if (transcriber_ && !seg.samples.empty()) {
      Transcript tx = transcriber_->Transcribe(
          seg.samples.data(), seg.samples.size(), 16000);
      AppendTranscript(seg, tx);
    }
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

void Pipeline::AppendTranscript(const SpeechSegment& seg,
                                const Transcript& tx) {
  if (tx.text.empty() || !md_file_.is_open()) return;

  md_file_ << "**[" << FormatSeconds(seg.start_sec) << " — "
           << FormatSeconds(seg.end_sec) << "]**\n"
           << tx.text << "\n\n";
  md_file_.flush();
  ++segments_written_;
}

}  // namespace tinto
