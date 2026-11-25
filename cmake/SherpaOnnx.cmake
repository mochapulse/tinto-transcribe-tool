include(FetchContent)

set(sherpa_version v2.0.3)
set(sherpa_release "https://github.com/k2-fsa/sherpa-onnx/releases/download/${sherpa_version}")
if(NOT CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$" OR
   NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The optional diarization package requires an x64 target")
endif()

if(WIN32)
  set(sherpa_archive "sherpa-onnx-${sherpa_version}-win-x64-shared-MT-Release-lib.tar.bz2")
  set(sherpa_sha 61a0b0371b54554c65068f994c83de482e78928fa9d0b7dfb7c35a451595e7fb)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  set(sherpa_archive "sherpa-onnx-${sherpa_version}-linux-x64-shared-lib.tar.bz2")
  set(sherpa_sha 898e710a00ae0ed64e82d8696f65e40969e18c65e664b41fa80c52c4d865bd33)
else()
  message(FATAL_ERROR "The optional diarization package supports Windows and Linux")
endif()

FetchContent_Declare(sherpa_runtime
  URL "${sherpa_release}/${sherpa_archive}"
  URL_HASH "SHA256=${sherpa_sha}")
FetchContent_MakeAvailable(sherpa_runtime)

set(sherpa_header_dir "${CMAKE_BINARY_DIR}/_deps/sherpa-include")
file(MAKE_DIRECTORY "${sherpa_header_dir}")
file(DOWNLOAD
  "https://raw.githubusercontent.com/k2-fsa/sherpa-onnx/${sherpa_version}/sherpa-onnx/c-api/c-api.h"
  "${sherpa_header_dir}/sherpa-onnx-c-api.h"
  EXPECTED_HASH SHA256=1f738de63a206328253ff471edda96b011fa1bd6f2a028f82967a7761abc2edd
  TLS_VERIFY ON TIMEOUT 60)

set(sherpa_lib "${sherpa_runtime_SOURCE_DIR}/lib")
add_library(tinto_sherpa SHARED IMPORTED GLOBAL)
set_target_properties(tinto_sherpa PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${sherpa_header_dir}")
if(WIN32)
  set_target_properties(tinto_sherpa PROPERTIES
    IMPORTED_IMPLIB "${sherpa_lib}/sherpa-onnx-c-api.lib"
    IMPORTED_LOCATION "${sherpa_lib}/sherpa-onnx-c-api.dll")
  set(TINTO_DIARIZATION_DLLS
    "${sherpa_lib}/sherpa-onnx-c-api.dll"
    "${sherpa_lib}/onnxruntime.dll"
    "${sherpa_lib}/onnxruntime_providers_shared.dll")
else()
  set_target_properties(tinto_sherpa PROPERTIES
    IMPORTED_LOCATION "${sherpa_lib}/libsherpa-onnx-c-api.so")
  set(TINTO_DIARIZATION_LIBS
    "${sherpa_lib}/libsherpa-onnx-c-api.so"
    "${sherpa_lib}/libonnxruntime.so")
endif()
