cmake_minimum_required(VERSION 3.13)
project(ffmpeg_transcoder)

set(CMAKE_CXX_STANDARD 14)

set(ffmpeg_inc_dirs /usr/local/include)
set(ffmpeg_link_dirs /usr/local/lib)

include_directories(${ffmpeg_inc_dirs})
link_directories(${ffmpeg_link_dirs})

add_executable(ffmpeg_transcoder
        ffmpeg_transcode.cpp
        main.cpp)
target_link_libraries(ffmpeg_transcoder avcodec avformat avutil swscale)