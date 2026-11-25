add_library(tinto_core STATIC
  src/paths.cpp
  src/config.cpp
  src/audio_mixer.cpp
  src/speech_window.cpp
  src/capture_miniaudio.cpp
  src/profile.cpp
  src/storage.cpp
  src/diarization_labels.cpp
  src/daemon.cpp
  src/models.cpp
  src/model_install.cpp
  src/pipeline.cpp
  src/whisper_info.cpp
)

# Generate version.hpp from the repo-root VERSION file (single source of truth).
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/tinto/version.hpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/tinto/version.hpp
  @ONLY)

target_include_directories(tinto_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_include_directories(tinto_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
if(MSVC)
  target_compile_options(tinto_core PRIVATE /W4 /utf-8)
  set(miniaudio_warning_option /w)
else()
  target_compile_options(tinto_core PRIVATE -Wall -Wextra)
  set(miniaudio_warning_option -w)
endif()

if(TINTO_WITH_DIARIZATION)
  target_sources(tinto_core PRIVATE src/diarization_sherpa.cpp)
  target_link_libraries(tinto_core PRIVATE tinto_sherpa)
  target_compile_definitions(tinto_core PUBLIC TINTO_WITH_DIARIZATION=1)
endif()

# Suppress diagnostics from vendored miniaudio in the files that include it.
set_source_files_properties(src/capture_miniaudio.cpp src/storage.cpp
  PROPERTIES COMPILE_OPTIONS "${miniaudio_warning_option}")

if(WIN32)
  target_link_libraries(tinto_core PRIVATE advapi32 bcrypt cfgmgr32 user32)
else()
  # miniaudio uses pthreads, libm and dlopen on Linux (PulseAudio/PipeWire).
  target_link_libraries(tinto_core PUBLIC pthread m dl)
endif()

# whisper.cpp dependency (ASR + Silero VAD), wired via FetchContent at the top.
if(TINTO_WITH_WHISPER)
  target_sources(tinto_core PRIVATE
    src/transcriber_whisper.cpp
    src/gate_whisper_vad.cpp)
  target_link_libraries(tinto_core PUBLIC whisper)
  target_compile_definitions(tinto_core PUBLIC TINTO_WITH_WHISPER=1)
endif()
