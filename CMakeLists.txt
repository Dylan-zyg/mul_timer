cmake_minimum_required(VERSION 2.8)
project (timerDemo)
aux_source_directory(./demo DIR_DEMO_SRCS)
add_subdirectory(src)

add_executable(Demo ${DIR_DEMO_SRCS})
target_link_libraries(Demo multimer)
target_link_libraries(Demo pthread)