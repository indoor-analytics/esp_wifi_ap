# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

if(NOT SSID_VALUE)
  message(FATAL_ERROR "No SSID value given. Please use 'idf.py build -DSSID_VALUE=<integer>'. This will set up your AP's SSID as 'ESP32_AP<SSID_VALUE>'")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(esp-wifi-ap)

