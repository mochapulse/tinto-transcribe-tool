add_executable(tinto main.cpp diarize.cpp diagnostics.cpp help.cpp)
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
install(TARGETS tinto RUNTIME DESTINATION . COMPONENT TintoRuntime)
if(TINTO_DIARIZATION_DLLS)
  install(FILES ${TINTO_DIARIZATION_DLLS} DESTINATION . COMPONENT TintoRuntime)
elseif(TINTO_DIARIZATION_LIBS)
  set_target_properties(tinto PROPERTIES INSTALL_RPATH "$ORIGIN/lib")
  install(FILES ${TINTO_DIARIZATION_LIBS} DESTINATION lib COMPONENT TintoRuntime)
endif()
if(TINTO_WITH_DIARIZATION)
  install(FILES "${PROJECT_SOURCE_DIR}/licenses/sherpa-onnx.txt"
                "${PROJECT_SOURCE_DIR}/licenses/onnx-runtime.txt"
    DESTINATION licenses COMPONENT TintoRuntime)
endif()
if(TINTO_WITH_WHISPER)
  install(FILES "${whisper_SOURCE_DIR}/LICENSE" DESTINATION licenses
    RENAME whisper.txt COMPONENT TintoRuntime)
endif()
