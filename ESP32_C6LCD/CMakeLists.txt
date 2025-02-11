# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)

set(
    ESXTRA_COMPONENTS_DIRS 
    "components/BLE"
    "components/get_bili"
    "components/led_strip" 
    "components/lvgl_driver" 
    "components/rgb_driver"
    "components/wifi"
    "components/lvgl"
    "components/UI"
    "components/SDcard"
    )
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ESP32_C6LCD)
