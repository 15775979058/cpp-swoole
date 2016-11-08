cmake_minimum_required(VERSION 2.4)
project(server)

set(SOURCE_FILES server.cpp)
SET(CMAKE_BUILD_TYPE Debug)

INCLUDE_DIRECTORIES(BEFORE ./include /opt/swoole/include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})

add_executable(server ${SOURCE_FILES})
target_link_libraries(server swoole_cpp swoole)

