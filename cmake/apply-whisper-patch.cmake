# FetchContent can rerun the patch step without resetting the source tree.
# Normalize the patch as well as pinning Git's checkout behavior. Some Windows
# runners otherwise feed a CRLF patch into the dependency's LF source tree.
file(READ "${PATCH_FILE}" patch_text)
string(REPLACE "\r\n" "\n" patch_text "${patch_text}")
set(normalized "${CMAKE_CURRENT_BINARY_DIR}/.tinto-whisper.patch")
file(WRITE "${normalized}" "${patch_text}")
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --check "${normalized}"
  RESULT_VARIABLE check_result ERROR_VARIABLE check_error)
if(check_result EQUAL 0)
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${normalized}"
    RESULT_VARIABLE patch_result)
  file(REMOVE "${normalized}")
  if(patch_result EQUAL 0)
    return()
  endif()
else()
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${normalized}"
    RESULT_VARIABLE reverse_result ERROR_QUIET)
  file(REMOVE "${normalized}")
  if(reverse_result EQUAL 0)
    return()
  endif()
endif()
message(FATAL_ERROR "Could not apply the Whisper inference cancellation patch\n${check_error}")
