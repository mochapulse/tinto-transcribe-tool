#pragma once

#include <atomic>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>

#include "tinto/audio_mixer.hpp"
#include "tinto/config.hpp"
#include "tinto/gate.hpp"
#include "tinto/speech_window.hpp"
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
  void Run(const CancelTranscription& stop_requested,
           const CancelTranscription& cancel_requested);

 private:
  void Finish(const CancelTranscription& should_cancel);
  void ProcessBuffer(bool final, const CancelTranscription& should_cancel);
  void WriteTranscriptHeader();
  void AppendTranscript(const SpeechSegment& seg, const Transcript& tx);

  Config cfg_;
  ModelPaths model_paths_;

  std::unique_ptr<ICapture> capture_;
  std::unique_ptr<IGate> gate_;
  std::unique_ptr<ITranscriber> transcriber_;
  WavWriter wav_writer_;

  AudioMixer mixer_;
  SpeechWindowBuffer windows_;
  std::atomic<bool> accept_audio_{false};

  std::string wav_path_;
  std::string wav_staging_path_;
  std::string md_path_;
  std::ofstream md_file_;

  std::size_t segments_written_ = 0;
  bool transcription_interrupted_ = false;
  bool aligned_wav_ok_ = true;
  bool started_ = false;
  bool finished_ = false;
};

}  // namespace tinto
