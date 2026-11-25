add_executable(tinto main.cpp diarize.cpp)
target_link_libraries(tinto PRIVATE tinto_core)
if(TINTO_DIARIZATION_DLLS)
  add_custom_command(TARGET tinto POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      ${TINTO_DIARIZATION_DLLS} "$<TARGET_FILE_DIR:tinto>"
    VERBATIM)
endif()
if(MSVC)
  target_compile_options(tinto PRIVATE /W4 /utf-8)
else()
  target_compile_options(tinto PRIVATE -Wall -Wextra)
endif()
