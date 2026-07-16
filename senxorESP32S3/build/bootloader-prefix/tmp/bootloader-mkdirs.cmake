# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "C:/Users/linzr/esp/v5.2.3/esp-idf/components/bootloader/subproject"
  "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader"
  "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix"
  "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix/tmp"
  "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix/src/bootloader-stamp"
  "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix/src"
  "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "H:/file_products/Thermal_Camera_ESP32 Module/Thermal-ESP32-Demo/Source_Codes/senxorESP32S3/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
