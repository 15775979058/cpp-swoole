cmake_minimum_required(VERSION 2.4)
project(swoole_cpp)

file(GLOB_RECURSE SOURCE_FILES FOLLOW_SYMLINKS src/*.cpp)
file(GLOB_RECURSE HEADER_FILES FOLLOW_SYMLINKS include/*.hpp)

set(SWOOLE_CPP_VERSION 1.9.0)
SET(CMAKE_BUILD_TYPE Debug)

INCLUDE_DIRECTORIES(BEFORE ./include /opt/swoole/include)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -O2 -Wall")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

SET(LIBRARY_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/lib)
add_library(libswoole_cpp SHARED ${SOURCE_FILES})
set_target_properties(libswoole_cpp PROPERTIES OUTPUT_NAME "swoole_cpp" VERSION ${SWOOLE_CPP_VERSION})
target_link_libraries(libswoole_cpp swoole)

#install
INSTALL(CODE "MESSAGE(\"Are you run command using root user?\")")
INSTALL(TARGETS libswoole_cpp LIBRARY DESTINATION lib ARCHIVE DESTINATION lib)
INSTALL(FILES ${HEADER_FILES} DESTINATION include/swoole)
