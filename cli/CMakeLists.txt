add_executable(tinto main.cpp)
target_link_libraries(tinto PRIVATE tinto_core)
target_compile_options(tinto PRIVATE -Wall -Wextra)
