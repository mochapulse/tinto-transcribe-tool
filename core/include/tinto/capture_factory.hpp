#pragma once

#include <memory>

#include "tinto/capture.hpp"

namespace tinto {

// Creates the shared miniaudio capture implementation.
std::unique_ptr<ICapture> MakeCapture();

}  // namespace tinto
