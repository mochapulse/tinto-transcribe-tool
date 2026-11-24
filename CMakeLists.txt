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

add_subdirectory(core)
add_subdirectory(cli)
