add_library(tinto_core STATIC
  src/paths.cpp
  src/config.cpp
  src/profile.cpp
)

target_include_directories(tinto_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_include_directories(tinto_core PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor)
target_compile_options(tinto_core PRIVATE -Wall -Wextra)
