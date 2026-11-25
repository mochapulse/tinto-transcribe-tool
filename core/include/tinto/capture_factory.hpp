#pragma once

#include <memory>

#include "tinto/capture.hpp"

namespace tinto {

struct CaptureProbe {
  std::string backend;
  unsigned inputs = 0;
  unsigned outputs = 0;
  bool enumerated = false;
};

// List audio endpoints without opening a capture or playback stream.
CaptureProbe ProbeCapture();

// Creates the shared miniaudio capture implementation.
std::unique_ptr<ICapture> MakeCapture();

}  // namespace tinto
