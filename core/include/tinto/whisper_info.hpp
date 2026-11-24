#pragma once

#include <string>

namespace tinto {

// Returns whisper.cpp system/build info (CPU feature flags, etc.). This both
// proves the whisper dependency links and is useful in `status`/logs. Returns
// an empty string when built without whisper (TINTO_WITH_WHISPER off).
std::string WhisperBuildInfo();

}  // namespace tinto
