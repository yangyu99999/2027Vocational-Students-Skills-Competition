cmake_minimum_required(VERSION 3.24)
set(CMAKE_TOOLCHAIN_FILE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/arm-gcc.cmake" CACHE FILEPATH "ARM toolchain")
project(smart_door LANGUAGES C ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include(cmake/sources.cmake)
add_executable(smart_door ${SMART_DOOR_SOURCES})
set_target_properties(smart_door PROPERTIES SUFFIX ".elf")
target_include_directories(smart_door PRIVATE ${SMART_DOOR_INCLUDES})
target_compile_definitions(smart_door PRIVATE USE_HAL_DRIVER STM32F103xB)
target_compile_options(smart_door PRIVATE -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
    -ffunction-sections -fdata-sections -fno-common
    $<$<COMPILE_LANGUAGE:C>:-fshort-enums>
    $<$<CONFIG:Debug>:-Og> $<$<CONFIG:Debug>:-g3>
    $<$<CONFIG:Release>:-Os>)
target_link_options(smart_door PRIVATE -mcpu=cortex-m3 -mthumb -mfloat-abi=soft
    -T${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F103C8_FLASH.ld
    --specs=nano.specs --specs=nosys.specs
    -Wl,--gc-sections -Wl,--print-memory-usage
    -Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/smart_door.map)
set_property(TARGET smart_door APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/STM32F103C8_FLASH.ld")
target_link_libraries(smart_door PRIVATE m)
add_custom_command(TARGET smart_door POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:smart_door> ${CMAKE_CURRENT_BINARY_DIR}/smart_door.hex
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:smart_door> ${CMAKE_CURRENT_BINARY_DIR}/smart_door.bin
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:smart_door> VERBATIM)
set(OPENOCD_EXECUTABLE "E:/openocd/xpack-openocd-0.12.0-7-win32-x64/xpack-openocd-0.12.0-7/bin/openocd.exe" CACHE FILEPATH "OpenOCD executable")
add_custom_target(flash
    COMMAND "${OPENOCD_EXECUTABLE}" -f "${CMAKE_CURRENT_SOURCE_DIR}/openocd/stlink.cfg"
        -c "program {$<TARGET_FILE:smart_door>} verify reset exit"
    DEPENDS smart_door USES_TERMINAL VERBATIM)
