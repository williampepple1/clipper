cmake_minimum_required(VERSION 3.20)
project(Clipper VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(FFMPEG_ROOT "" CACHE PATH "FFmpeg install prefix")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include" CACHE PATH "FFmpeg include dir")
set(FFMPEG_LIB_DIR "${FFMPEG_ROOT}/lib" CACHE PATH "FFmpeg lib dir")

find_library(AVCODEC_LIB  avcodec  PATHS "${FFMPEG_LIB_DIR}" REQUIRED)
find_library(AVFORMAT_LIB avformat PATHS "${FFMPEG_LIB_DIR}" REQUIRED)
find_library(AVUTIL_LIB   avutil   PATHS "${FFMPEG_LIB_DIR}" REQUIRED)
find_library(SWSCALE_LIB  swscale  PATHS "${FFMPEG_LIB_DIR}" REQUIRED)
find_library(SWRESAMPLE_LIB swresample PATHS "${FFMPEG_LIB_DIR}" REQUIRED)

add_executable(clipper
    src/main.cpp
    src/mainwindow.h    src/mainwindow.cpp
    src/screencapturer.h src/screencapturer.cpp
    src/audiocapturer.h  src/audiocapturer.cpp
    src/encoder.h        src/encoder.cpp
    src/recorder.h       src/recorder.cpp
    src/regionselector.h src/regionselector.cpp
    src/hotkeymanager.h  src/hotkeymanager.cpp
    src/previewwindow.h  src/previewwindow.cpp
    src/recordingindicator.h src/recordingindicator.cpp
    src/timelineslider.h src/timelineslider.cpp
    src/videotrimmer.h   src/videotrimmer.cpp
    src/platformpresets.h
)

target_include_directories(clipper PRIVATE ${FFMPEG_INCLUDE_DIR} src)

target_link_libraries(clipper PRIVATE
    Qt6::Widgets
    ${AVCODEC_LIB} ${AVFORMAT_LIB} ${AVUTIL_LIB} ${SWSCALE_LIB} ${SWRESAMPLE_LIB}
    d3d11 dxgi ole32 avrt user32 uuid
)
