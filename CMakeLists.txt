cmake_minimum_required(VERSION 3.14)
project(ffmpeg-study C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(lib/ffmpeg)

list(APPEND SOURCES
  src/01_example_scanning.cpp
)

add_executable(ffmpeg-study ${SOURCES})
target_link_libraries(ffmpeg-study FFmpeg ${EXTRA_LIBS})