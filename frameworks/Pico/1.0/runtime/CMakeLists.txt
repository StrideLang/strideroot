cmake_minimum_required(VERSION 3.13)

# PICO_BOARD (e.g. pico or pico2) and PICO_SDK_PATH must be set before project().
if(DEFINED PICO_SDK_PATH AND EXISTS "${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  include("${PICO_SDK_PATH}/external/pico_sdk_import.cmake")
elseif(EXISTS "$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
  include("$ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake")
else()
  message(FATAL_ERROR "pico_sdk_import.cmake not found; set PICO_SDK_PATH to the pico-sdk directory")
endif()

project(stride_pico_firmware C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

set(STRIDE_PROGRAM_OBJ "" CACHE FILEPATH "Compiled Stride program object file")
set(STRIDE_STRING_RUNTIME "" CACHE FILEPATH "Path to strideruntime_string.c")
set(STRIDE_JIT_INCLUDE_DIR "" CACHE PATH "Path to stridejit include directory")
set(STRIDE_RUNTIME_C "${CMAKE_CURRENT_LIST_DIR}/stride_pico_runtime.c" CACHE FILEPATH "Path to stride_pico_runtime.c")

add_executable(stride_firmware
  "${STRIDE_RUNTIME_C}"
  "${STRIDE_STRING_RUNTIME}"
  "${STRIDE_PROGRAM_OBJ}"
)

target_include_directories(stride_firmware PRIVATE "${STRIDE_JIT_INCLUDE_DIR}")
target_compile_definitions(stride_firmware PRIVATE STRIDE_RT_REAL_IS_FLOAT=1)

# Enable USB CDC stdio by default; UART stdio can also be enabled via board config.
pico_enable_stdio_usb(stride_firmware 1)
pico_enable_stdio_uart(stride_firmware 0)

target_link_libraries(stride_firmware pico_stdlib m)
pico_add_extra_outputs(stride_firmware)
