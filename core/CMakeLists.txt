add_library(tinto_core STATIC
  src/paths.cpp
  src/config.cpp
  src/capture_miniaudio.cpp
)

target_include_directories(tinto_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_options(tinto_core PRIVATE -Wall -Wextra)

# The vendored miniaudio.h is a single large header that triggers warnings we
# do not own; silence diagnostics for that translation unit only.
set_source_files_properties(src/capture_miniaudio.cpp PROPERTIES COMPILE_OPTIONS "-w")

# miniaudio uses pthreads, libm and dlopen on Linux (PulseAudio/PipeWire backend).
target_link_libraries(tinto_core PUBLIC pthread m dl)
