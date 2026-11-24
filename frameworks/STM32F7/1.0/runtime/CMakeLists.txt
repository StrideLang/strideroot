cmake_minimum_required(VERSION 3.13)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

if(NOT CMAKE_C_COMPILER)
  set(CMAKE_C_COMPILER arm-none-eabi-gcc)
endif()
if(NOT CMAKE_ASM_COMPILER)
  set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
endif()
if(NOT CMAKE_OBJCOPY)
  set(CMAKE_OBJCOPY arm-none-eabi-objcopy)
endif()

project(stride_stm32f7_firmware C ASM)
set(CMAKE_C_STANDARD 11)

set(STRIDE_STM32F7_DEVICE "STM32F769xx" CACHE STRING "STM32F7 device define (e.g. STM32F769xx, STM32F767xx, STM32F746xx)")
set(STRIDE_STM32F7_CPU "cortex-m7" CACHE STRING "ARM CPU target")
set(STRIDE_STM32F7_FPU "fpv5-d16" CACHE STRING "ARM FPU target (fpv5-d16 or fpv5-sp-d16)")
set(STRIDE_STM32F7_FLASH_KB "2048" CACHE STRING "Internal Flash size in KB")
set(STRIDE_STM32F7_RAM_KB "512" CACHE STRING "Internal SRAM size in KB")
set(STRIDE_STM32F7_CLOCK_HZ "16000000" CACHE STRING "System clock frequency in Hz")
set(STRIDE_STM32F7_UART "3" CACHE STRING "Console USART instance (1=USART1 PA9/PB7 for F746G-DISCO, 3=USART3 PD8/PD9 for Nucleo)")

set(STRIDE_PROGRAM_OBJ "" CACHE FILEPATH "Compiled Stride program object file")
set(STRIDE_STRING_RUNTIME "" CACHE FILEPATH "Path to strideruntime_string.c")
set(STRIDE_JIT_INCLUDE_DIR "" CACHE PATH "Path to stridejit include directory")
set(STRIDE_RUNTIME_C "${CMAKE_CURRENT_LIST_DIR}/stride_stm32f7_runtime.c" CACHE FILEPATH "Path to stride_stm32f7_runtime.c")
set(STRIDE_STARTUP_C "${CMAKE_CURRENT_LIST_DIR}/startup_stm32f7.c" CACHE FILEPATH "Path to startup_stm32f7.c")
set(STRIDE_STM32F7_LINKER_SCRIPT "" CACHE FILEPATH "Optional custom linker script path")

if(NOT STRIDE_STM32F7_LINKER_SCRIPT)
  set(_generated_ld "${CMAKE_CURRENT_BINARY_DIR}/stm32f7_flash.ld")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/stm32f7_flash.ld.in" "${_generated_ld}" @ONLY)
  set(STRIDE_STM32F7_LINKER_SCRIPT "${_generated_ld}")
endif()

set(_arch_flags
  "-mcpu=${STRIDE_STM32F7_CPU}"
  "-mthumb"
  "-mfpu=${STRIDE_STM32F7_FPU}"
  "-mfloat-abi=hard"
)

add_executable(stride_firmware
  "${STRIDE_STARTUP_C}"
  "${STRIDE_RUNTIME_C}"
  "${STRIDE_STRING_RUNTIME}"
  "${STRIDE_PROGRAM_OBJ}"
)

set_target_properties(stride_firmware PROPERTIES SUFFIX ".elf")

target_include_directories(stride_firmware PRIVATE "${STRIDE_JIT_INCLUDE_DIR}")
target_compile_definitions(stride_firmware PRIVATE
  "${STRIDE_STM32F7_DEVICE}"
  "STRIDE_RT_REAL_IS_FLOAT=1"
  "STRIDE_STM32F7_SYSCLK_HZ=${STRIDE_STM32F7_CLOCK_HZ}UL"
  "STRIDE_STM32F7_UART=${STRIDE_STM32F7_UART}"
)
target_compile_options(stride_firmware PRIVATE
  ${_arch_flags}
  -ffunction-sections
  -fdata-sections
  -Wall
)
target_link_options(stride_firmware PRIVATE
  ${_arch_flags}
  -specs=nosys.specs
  -specs=nano.specs
  "-T${STRIDE_STM32F7_LINKER_SCRIPT}"
  -Wl,--gc-sections
)
target_link_libraries(stride_firmware PRIVATE m)

add_custom_command(TARGET stride_firmware POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:stride_firmware>" "${CMAKE_CURRENT_BINARY_DIR}/stride_firmware.bin"
  COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:stride_firmware>" "${CMAKE_CURRENT_BINARY_DIR}/stride_firmware.hex"
  COMMENT "Generating stride_firmware.bin and stride_firmware.hex"
)
