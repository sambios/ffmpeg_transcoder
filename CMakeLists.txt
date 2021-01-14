cmake_minimum_required(VERSION 3.2)
project(ffmpeg_transcoder)

if (APPLE)
    option(USE_BMCODEC "Use bitmain hardware codec" OFF)
    option(USE_OUTPUT_FILE "Output as h264 file" OFF)
    option(USE_FFMPEG_STATIC "Use ffmpeg static library" OFF)
else ()
    option(USE_BMCODEC "Use bitmain hardware codec" ON)
    option(USE_OUTPUT_FILE "Output as h264 file" OFF)
    option(USE_FFMPEG_STATIC "Use ffmpeg static library" ON)
    option(USE_DEV "Use develop version" OFF)
endif ()

if (USE_OUTPUT_FILE)
    add_definitions(-DUSE_OUTPUT_FILE=1)
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CFLAGS_RELEASE -O2)

set(TOP ${CMAKE_CURRENT_SOURCE_DIR})
if (USE_BMCODEC)
    if (USE_FFMPEG_STATIC) # Use ffmpeg static library
       set(FFMPEG_ROOT ${TOP}/../bm_ffmpeg_static_sdk)
    else(USE_FFMPEG_STATIC) # Don't use ffmpeg static
        if (USE_DEV) # Use develop branch
            set(FFMPEG_ROOT /home/yuan/bitm/middleware-soc/install/pcie_bm1684_asic/ffmpeg/usr/local)
            set(BM_DECODE_ROOT /home/yuan/bitm/middleware-soc/install/pcie_bm1684_asic/decode)
        else()
            set(FFMPEG_ROOT ${TOP}/../bm_ffmpeg_static_sdk)
        endif()
    endif()
    add_definitions(-DUSE_BMCODEC=1)
else()
    if (APPLE)
        set(FFMPEG_ROOT /usr/local)
    else()
        set(FFMPEG_ROOT /usr/include/x86_64-linux-gnu)
    endif()
endif()

set(ffmpeg_inc_dirs ${FFMPEG_ROOT}/include)
set(ffmpeg_link_dirs ${FFMPEG_ROOT}/lib)

if (USE_DEV)
    list_append(ffmpeg_link_dirs ${BM_DECODE_ROOT}/lib)
endif()

message(STATUS "ffmpeg_link_dirs=${ffmpeg_link_dirs}")

if (USE_FFMPEG_STATIC)
set(ffmpeg_link_libs
    libavfilter.a libavformat.a libavcodec.a libavutil.a libswresample.a libswscale.a
    bmjpulite bmjpuapi bmvpulite bmvpuapi bmvideo bmvppapi bmion bmlib
    osip ssl crypto pthread freetype dl m z)
else()
    set(ffmpeg_link_libs avfilter avformat avcodec avutil swresample swscale pthread)
endif()

include_directories(${ffmpeg_inc_dirs})
link_directories(${ffmpeg_link_dirs})

add_executable(fftrans
        ffmpeg_transcode.cpp
        main.cpp)
target_link_libraries(fftrans ${ffmpeg_link_libs})


#find_package(OpenCV)
#add_executable(videoplayer
#        ffmpeg_transcode.cpp
#        video_player.cpp)
#target_link_libraries(videoplayer ${ffmpeg_link_libs} ${OpenCV_LIBS})
