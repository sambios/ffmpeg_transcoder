cmake_minimum_required(VERSION 3.13)
project(ffmpeg_transcoder)

option(USE_BMCODEC "Use bitmain hardware codec" ON)

set(CMAKE_CXX_STANDARD 14)

if (USE_BMCODEC)
    set(FFMPEG_ROOT $ENV{HOME}/bm_ffmpeg_sdk)
    add_definitions(-DUSE_BMCODEC=1)
else()
    if (APPLE)
        set(FFMPEG_ROOT /usr/local)
    else()
        set(FFMPEG_ROOT $ENV{HOME}/third_party)
    endif()
endif()

set(ffmpeg_inc_dirs ${FFMPEG_ROOT}/include)
set(ffmpeg_link_dirs ${FFMPEG_ROOT}/lib)

include_directories(${ffmpeg_inc_dirs})
link_directories(${ffmpeg_link_dirs})

add_executable(fftrans
        ffmpeg_transcode.cpp
        main.cpp)
target_link_libraries(fftrans avcodec avformat avutil avfilter swscale pthread)
