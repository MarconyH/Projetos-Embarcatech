# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(FatFS_SPI_example C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise the Pico SDK
pico_sdk_init()

add_subdirectory(../FatFs_SPI build)

# Add executable. Default name is the project name, version 0.1
add_executable(FatFS_SPI_example 
    FatFS_SPI_example.cpp 
    hw_config.c
    data_log_demo.c
    tests/simple.c
    tests/app4-IO_module_function_checker.c
    tests/big_file_test.c
    tests/CreateAndVerifyExampleFiles.c
    tests/ff_stdio_tests_with_cwd.c
)
# Add the standard library to the build
target_link_libraries(FatFS_SPI_example pico_stdlib)

target_compile_options(FatFS_SPI_example PUBLIC -Wall -Wextra -Wno-unused-function -Wno-unused-parameter)

# Use Pico's LED to show drive activity. 
# Ensure that PICO_DEFAULT_LED_PIN is set correctly.
# Note that Pico W uses GPIO 25 for SPI communication to the CYW43439.
# add_compile_definitions(USE_LED=1)

pico_set_program_name(FatFS_SPI_example "FatFS_SPI_example")
pico_set_program_version(FatFS_SPI_example "0.1")

# See 4.1. Serial input and output on Raspberry Pi Pico in Getting started with Raspberry Pi Pico (https://datasheets.raspberrypi.org/pico/getting-started-with-pico.pdf)
# and 2.7.1. Standard Input/Output (stdio) Support in Raspberry Pi Pico C/C++ SDK (https://datasheets.raspberrypi.org/pico/raspberry-pi-pico-c-sdk.pdf):
pico_enable_stdio_uart(FatFS_SPI_example 1)
pico_enable_stdio_usb(FatFS_SPI_example 1)

target_link_libraries(FatFS_SPI_example
    FatFs_SPI
    hardware_clocks
    hardware_adc
)

pico_add_extra_outputs(FatFS_SPI_example)

