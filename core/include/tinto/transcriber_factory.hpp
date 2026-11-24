#pragma once

#include <memory>
#include <string>

#include "tinto/transcriber.hpp"

namespace tinto {

// Creates a local whisper.cpp transcriber from a ggml model file.
// `lang` is a whisper language code (e.g. "es", "en"). `threads` <= 0 selects
// the hardware concurrency automatically. If the model fails to load, the
// returned transcriber produces empty transcripts.
std::unique_ptr<ITranscriber> MakeWhisperTranscriber(const std::string& model_path,
                                                     const std::string& lang = "es",
                                                     int threads = 0);

}  // namespace tinto
