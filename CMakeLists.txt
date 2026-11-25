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
if(WIN32)
  # Keep the CLI runnable from its build directory without copying whisper DLLs.
  option(BUILD_SHARED_LIBS "Build dependencies as shared libraries" OFF)
endif()
if(TINTO_WITH_WHISPER)
  include(FetchContent)
  find_package(Git REQUIRED)
  set(WHISPER_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(WHISPER_BUILD_SERVER OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(whisper
    GIT_REPOSITORY https://github.com/ggml-org/whisper.cpp.git
    GIT_TAG c1a24047871444999b79f5e8612828f45881ae12
    GIT_CONFIG core.autocrlf=false
    PATCH_COMMAND ${CMAKE_COMMAND}
      "-DGIT_EXECUTABLE=${GIT_EXECUTABLE}"
      "-DPATCH_FILE=${CMAKE_CURRENT_SOURCE_DIR}/cmake/whisper-cancel.patch"
      -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/apply-whisper-patch.cmake")
  FetchContent_MakeAvailable(whisper)
endif()

option(TINTO_WITH_DIARIZATION "Build with local offline speaker diarization" OFF)
if(TINTO_WITH_DIARIZATION)
  include(cmake/SherpaOnnx.cmake)
endif()

add_subdirectory(core)
add_subdirectory(cli)

include(CTest)
if(BUILD_TESTING)
  set(TINTO_TEST_ASR_MODEL "" CACHE FILEPATH "Local Whisper model for the cancellation test")
  if(TINTO_WITH_WHISPER AND TINTO_TEST_ASR_MODEL)
    add_executable(whisper-cancel-test tests/whisper-cancel.cpp)
    target_link_libraries(whisper-cancel-test PRIVATE tinto_core)
    add_test(NAME whisper-cancel COMMAND whisper-cancel-test "${TINTO_TEST_ASR_MODEL}")
    set_tests_properties(whisper-cancel PROPERTIES TIMEOUT 20)
  endif()
  add_executable(audio-mixer-test tests/audio-mixer.cpp)
  target_link_libraries(audio-mixer-test PRIVATE tinto_core)
  add_test(NAME audio-mixer COMMAND audio-mixer-test)
  add_executable(speech-window-test tests/speech-window.cpp)
  target_link_libraries(speech-window-test PRIVATE tinto_core)
  add_test(NAME speech-window COMMAND speech-window-test)
  add_executable(diarization-test tests/diarization.cpp)
  target_link_libraries(diarization-test PRIVATE tinto_core)
  if(MSVC)
    target_compile_options(diarization-test PRIVATE /utf-8)
  endif()
  add_test(NAME diarization-alignment COMMAND diarization-test
    "${CMAKE_CURRENT_BINARY_DIR}/test-aligned.wav")
  add_executable(model-cache-test tests/model-cache.cpp)
  target_link_libraries(model-cache-test PRIVATE tinto_core)
  add_test(NAME model-cache COMMAND model-cache-test
    "${CMAKE_CURRENT_BINARY_DIR}/test model cache.bin")
  add_test(NAME cli-smoke COMMAND ${CMAKE_COMMAND}
    -DTINTO_EXE=$<TARGET_FILE:tinto>
    -DTINTO_VERSION=${TINTO_VERSION}
    -DTINTO_WITH_WHISPER=${TINTO_WITH_WHISPER}
    -DTINTO_WITH_DIARIZATION=${TINTO_WITH_DIARIZATION}
    -DTEST_HOME=${CMAKE_CURRENT_BINARY_DIR}/test-home
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cli-smoke.cmake)
endif()
