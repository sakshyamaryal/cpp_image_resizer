cmake_minimum_required(VERSION 3.10)
project(ImageResizer)

find_package(FFmpeg REQUIRED)

include_directories(include ${FFmpeg_INCLUDE_DIRS})

add_library(imageresizer src/ImageResizer.cpp)

add_executable(main main.cpp)
target_link_libraries(main imageresizer ${FFmpeg_LIBRARIES})
