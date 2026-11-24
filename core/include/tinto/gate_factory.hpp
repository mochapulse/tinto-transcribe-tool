#pragma once

#include <memory>
#include <string>

#include "tinto/gate.hpp"

namespace tinto {

// Creates a speech gate backed by whisper.cpp's built-in Silero VAD. The model
// is loaded from `vad_model_path`; if loading fails the gate is still returned
// and simply yields no segments.
std::unique_ptr<IGate> MakeWhisperVadGate(const std::string& vad_model_path);

}  // namespace tinto
