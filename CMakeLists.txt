cmake_minimum_required(VERSION 3.14)
project(ffmpeg-study C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_BUILD_TYPE Debug)

add_subdirectory(lib/ffmpeg)

link_libraries(FFmpeg)

file(GLOB srcs src/*.cpp)

foreach(src ${srcs})
  get_filename_component(TARGET ${src} NAME_WE)
  add_executable(${TARGET} ${src})
  message(STATUS "${TARGET} added")
endforeach()