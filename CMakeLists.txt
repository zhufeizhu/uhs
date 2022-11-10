cmake_minimum_required(VERSION 3.0.0)

project(mix)

set(CMAKE_C_STANDARD 11)

aux_source_directory(./src SRC_LIST)

include_directories(include)

add_definitions(-Werror -g -msse2) #支持pmdk指令

add_executable(mixdk ${SRC_LIST})

target_link_libraries(mixdk pthread m pmem)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)