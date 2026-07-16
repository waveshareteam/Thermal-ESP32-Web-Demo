# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/linzr/esp/v5.2.3/esp-idf/components/bootloader/subproject"
  "H:/github repository/Thermal-ESP32-Demo/build/bootloader"
  "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix"
  "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix/tmp"
  "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix/src/bootloader-stamp"
  "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix/src"
  "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "H:/github repository/Thermal-ESP32-Demo/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
