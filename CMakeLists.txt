cmake_minimum_required(VERSION 3.16)
project(tinto LANGUAGES CXX)

# Single source of truth for the version: the repo-root VERSION file.
# core/ configures include/tinto/version.hpp.in with ${TINTO_VERSION}.
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" TINTO_VERSION LIMIT_COUNT 1)
string(STRIP "${TINTO_VERSION}" TINTO_VERSION)
# Re-run CMake configuration (and regenerate version.hpp) when VERSION changes.
set_property(DIRECTORY APPEND PROPERTY
  CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# whisper.cpp provides both the ASR transcription and the built-in Silero VAD.
# Pinned to the exact commit validated in examples/RTF-VAD for reproducibility.
option(TINTO_WITH_WHISPER "Build with whisper.cpp (ASR + VAD)" ON)
if(TINTO_WITH_WHISPER)
  include(FetchContent)
  set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG c1a24047871444999b79f5e8612828f45881ae12)
  FetchContent_MakeAvailable(whisper)
endif()

add_subdirectory(core)
add_subdirectory(cli)
