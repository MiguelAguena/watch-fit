cmake_minimum_required(VERSION 3.16)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
set(PROJECT_NAME "esp_c6_cpp_app")
# Tell IDF we use C and C++
project(${PROJECT_NAME} C CXX)
# IDF handles toolchain/ld/startup/bootloader/partitions for ESP32-C6
