#pragma once

#include <atomic>
#include <cstddef>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "tinto/capture.hpp"
#include "tinto/config.hpp"
#include "tinto/gate.hpp"
#include "tinto/storage.hpp"
#include "tinto/transcriber.hpp"

namespace tinto {

class Pipeline {
 public:
  struct ModelPaths {
    std::string asr_model;
    std::string vad_model;
  };

  Pipeline(Config cfg, ModelPaths paths);
  ~Pipeline();

  Pipeline(const Pipeline&) = delete;
  Pipeline& operator=(const Pipeline&) = delete;

  bool Start();
  void Run(std::atomic<bool>& stop_flag);
  void Finish();

 private:
  void OnAudio(const AudioFrame& frame);
  void ProcessBuffer();
  void WriteTranscriptHeader();
  void AppendTranscript(const SpeechSegment& seg, const Transcript& tx);

  Config cfg_;
  ModelPaths model_paths_;

  std::unique_ptr<ICapture> capture_;
  std::unique_ptr<IGate> gate_;
  std::unique_ptr<ITranscriber> transcriber_;
  WavWriter wav_writer_;

  std::mutex audio_mutex_;
  std::vector<float> audio_buf_;

  std::string wav_path_;
  std::string md_path_;
  std::ofstream md_file_;

  std::size_t segments_written_ = 0;
  bool finished_ = false;
};

}  // namespace tinto
