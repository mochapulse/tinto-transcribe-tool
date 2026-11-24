#include "tinto/whisper_info.hpp"

#ifdef TINTO_WITH_WHISPER
#include <whisper.h>
#endif

namespace tinto {

std::string WhisperBuildInfo() {
#ifdef TINTO_WITH_WHISPER
  const char* info = whisper_print_system_info();
  return info != nullptr ? std::string(info) : std::string();
#else
  return std::string();
#endif
}

}  // namespace tinto
