cmake_minimum_required(VERSION 3.13)

# Firmware for STM32 boards: the program object compiled by Stride, linked
# with this runtime. `stride build` passes every value below from the board's
# Stride data, and writes STRIDE_BOARD_HEADER (stride_board.h: clocks and
# console) from it. Nothing here names a chip.

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

project(stride_stm32_firmware C ASM)
set(CMAKE_C_STANDARD 11)

set(STRIDE_CPU "" CACHE STRING "ARM CPU (device cpu), e.g. cortex-m4")
set(STRIDE_FPU "" CACHE STRING "FPU (device fpu), e.g. fpv4-sp-d16; empty for none")
set(STRIDE_FLASH_KB "" CACHE STRING "Internal flash in KB (device memory.flash)")
set(STRIDE_RAM_KB "" CACHE STRING "Internal SRAM at 0x20000000 in KB (device memory.ram)")
set(STRIDE_BOARD_HEADER "" CACHE FILEPATH "stride_board.h written by stride build")

set(STRIDE_PROGRAM_OBJ "" CACHE FILEPATH "Compiled Stride program object file")
set(STRIDE_STRING_RUNTIME "" CACHE FILEPATH "Path to strideruntime_string.c")
set(STRIDE_JIT_INCLUDE_DIR "" CACHE PATH "Path to stridejit include directory")
set(STRIDE_LINKER_SCRIPT "" CACHE FILEPATH "Optional custom linker script")

foreach(_required STRIDE_CPU STRIDE_FLASH_KB STRIDE_RAM_KB STRIDE_BOARD_HEADER)
  if(NOT ${_required})
    message(FATAL_ERROR "${_required} is not set; build with `stride build`")
  endif()
endforeach()

if(NOT STRIDE_LINKER_SCRIPT)
  set(_linker_script "${CMAKE_CURRENT_BINARY_DIR}/stm32_flash.ld")
  configure_file("${CMAKE_CURRENT_LIST_DIR}/stm32_flash.ld.in" "${_linker_script}" @ONLY)
else()
  set(_linker_script "${STRIDE_LINKER_SCRIPT}")
endif()

# Floating point: with an FPU, Stride's code (triple ...-eabihf) and this
# runtime use the hard-float ABI, and GCC links the matching C library
# multilib (thumb/v7e-m+fp/hard, thumb/v7e-m+dp/hard).
if(STRIDE_FPU)
  set(_fp_flags "-mfpu=${STRIDE_FPU}" "-mfloat-abi=hard")
else()
  set(_fp_flags "-mfloat-abi=soft")
endif()
set(_arch_flags "-mcpu=${STRIDE_CPU}" "-mthumb" ${_fp_flags})

add_executable(stride_firmware
  "${CMAKE_CURRENT_LIST_DIR}/startup_stm32.c"
  "${CMAKE_CURRENT_LIST_DIR}/stride_stm32_runtime.c"
  "${STRIDE_STRING_RUNTIME}"
  "${STRIDE_PROGRAM_OBJ}"
)
set_target_properties(stride_firmware PROPERTIES SUFFIX ".elf")
set_source_files_properties(
  "${CMAKE_CURRENT_LIST_DIR}/stride_stm32_runtime.c"
  PROPERTIES OBJECT_DEPENDS "${STRIDE_BOARD_HEADER}")

target_include_directories(stride_firmware PRIVATE "${STRIDE_JIT_INCLUDE_DIR}")
target_compile_definitions(stride_firmware PRIVATE "STRIDE_RT_REAL_IS_FLOAT=1")
target_compile_options(stride_firmware PRIVATE
  ${_arch_flags}
  "$<$<COMPILE_LANGUAGE:C>:SHELL:-include \"${STRIDE_BOARD_HEADER}\">"
  -ffunction-sections
  -fdata-sections
  -Wall
)
target_link_options(stride_firmware PRIVATE
  ${_arch_flags}
  -specs=nosys.specs
  -specs=nano.specs
  "-T${_linker_script}"
  -Wl,--gc-sections
  -Wl,-z,noexecstack
)
target_link_libraries(stride_firmware PRIVATE m)

add_custom_command(TARGET stride_firmware POST_BUILD
  COMMAND "${CMAKE_OBJCOPY}" -O binary "$<TARGET_FILE:stride_firmware>" "${CMAKE_CURRENT_BINARY_DIR}/stride_firmware.bin"
  COMMAND "${CMAKE_OBJCOPY}" -O ihex "$<TARGET_FILE:stride_firmware>" "${CMAKE_CURRENT_BINARY_DIR}/stride_firmware.hex"
  COMMENT "Generating stride_firmware.bin and stride_firmware.hex"
)
