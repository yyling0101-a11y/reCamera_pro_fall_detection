cmake_minimum_required(VERSION 3.16)
project(recamera_fall_detection LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SKIP_BUILD_RPATH ON)

if(NOT DEFINED RECAMERA_RKNNRT OR NOT EXISTS "${RECAMERA_RKNNRT}")
  message(FATAL_ERROR "Set RECAMERA_RKNNRT to the local board-compatible librknnrt.so")
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED IMPORTED_TARGET gstreamer-1.0 gstreamer-app-1.0)
find_library(GST_RTSP_SERVER_LIBRARY gstrtspserver-1.0
  HINTS "${CMAKE_SYSROOT}/usr/lib" "${CMAKE_SYSROOT}/usr/lib64" REQUIRED)
find_path(RKNN_INCLUDE_DIR rknn_api.h
  HINTS "${CMAKE_CURRENT_SOURCE_DIR}/include"
  REQUIRED)

add_executable(recamera_rknn_gst
  src/main.cpp
  src/fall_alarm.cpp
  src/fall_detector.cpp
  src/frame_overlay.cpp
  src/rknn_pose_model.cpp
  src/rtsp_streamer.cpp
  src/websocket_server.cpp)
target_link_directories(recamera_rknn_gst BEFORE PRIVATE
  "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu"
  "${CMAKE_SYSROOT}/lib")
target_compile_options(recamera_rknn_gst PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(recamera_rknn_gst PRIVATE "${RKNN_INCLUDE_DIR}")
target_include_directories(recamera_rknn_gst PRIVATE
  "${CMAKE_CURRENT_SOURCE_DIR}/toolchain/rtsp-server-dev/usr/include/gstreamer-1.0")
target_link_libraries(recamera_rknn_gst PRIVATE
  PkgConfig::GST
  "${GST_RTSP_SERVER_LIBRARY}"
  "${RECAMERA_RKNNRT}"
  "${CMAKE_SYSROOT}/lib/libc.so.6")
target_link_options(recamera_rknn_gst PRIVATE
  "LINKER:-rpath,/oem/usr/lib:/userdata/rknn_test/3rdparty/lib")
set_target_properties(recamera_rknn_gst PROPERTIES
  OUTPUT_NAME recamera_fall_detection
  SKIP_BUILD_RPATH TRUE)
add_custom_command(TARGET recamera_rknn_gst POST_BUILD
  COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:recamera_rknn_gst>"
          "${CMAKE_CURRENT_BINARY_DIR}/recamera_rknn_gst")
