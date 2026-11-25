# FetchContent can rerun the patch step without resetting the source tree.
execute_process(COMMAND "${GIT_EXECUTABLE}" apply --check "${PATCH_FILE}"
  RESULT_VARIABLE check_result ERROR_QUIET)
if(check_result EQUAL 0)
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply "${PATCH_FILE}"
    RESULT_VARIABLE patch_result)
  if(patch_result EQUAL 0)
    return()
  endif()
else()
  execute_process(COMMAND "${GIT_EXECUTABLE}" apply --reverse --check "${PATCH_FILE}"
    RESULT_VARIABLE reverse_result ERROR_QUIET)
  if(reverse_result EQUAL 0)
    return()
  endif()
endif()
message(FATAL_ERROR "Could not apply the Whisper inference cancellation patch")
