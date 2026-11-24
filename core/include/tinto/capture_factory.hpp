#pragma once

#include <memory>

#include "tinto/capture.hpp"

namespace tinto {

// Creates the platform capture implementation (miniaudio backend on Linux).
std::unique_ptr<ICapture> MakeCapture();

}  // namespace tinto
