add_library(tinto_core STATIC
  src/paths.cpp
  src/config.cpp
  src/capture_miniaudio.cpp
  src/profile.cpp
  src/storage.cpp
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
target_compile_options(tinto_core PRIVATE -Wall -Wextra)

# The vendored miniaudio.h is a single large header that triggers warnings we
# do not own; silence diagnostics for that translation unit only.
set_source_files_properties(src/capture_miniaudio.cpp PROPERTIES COMPILE_OPTIONS "-w")
# storage.cpp includes the same vendored header (declarations only); mute it too.
set_source_files_properties(src/storage.cpp PROPERTIES COMPILE_OPTIONS "-w")

# miniaudio uses pthreads, libm and dlopen on Linux (PulseAudio/PipeWire backend).
target_link_libraries(tinto_core PUBLIC pthread m dl)
