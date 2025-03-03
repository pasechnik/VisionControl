cmake_minimum_required(VERSION 3.16)
project(VisionControl)

set(CMAKE_CXX_STANDARD 17)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GSTREAMER_1_0 REQUIRED gstreamer-1.0)

include_directories(${GSTREAMER_1_0_INCLUDE_DIRS})

add_executable(VisionControl src/main.cpp)

target_link_libraries(VisionControl ${GSTREAMER_1_0_LIBRARIES})