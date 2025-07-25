# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

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
set(PICO_BOARD pico_w CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(projeto_pratico_etapa_1 C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1
add_subdirectory(no-OS-FatFS-SD-SPI-RPi-Pico/FatFs_SPI build)

add_executable(projeto_pratico_etapa_1 projeto_pratico_etapa_1.c 
inc/rfid/mfrc522.c
inc/rfid/tag_data_handler.c
inc/sd_card/sd_card_handler.c
inc/sd_card/hw_config.c
inc/spi_manager.c
WIFI_/fila_circular.c
WIFI_/conexao.c
WIFI_/mqtt_lwip.c
OLED_/display.c
OLED_/oled_utils.c
OLED_/ssd1306_i2c.c
OLED_/setup_oled.c
main_core1.c
)

pico_set_program_name(projeto_pratico_etapa_1 "projeto_pratico_etapa_1")
pico_set_program_version(projeto_pratico_etapa_1 "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(projeto_pratico_etapa_1 0)
pico_enable_stdio_usb(projeto_pratico_etapa_1 1)

# Add the standard library to the build
target_link_libraries(projeto_pratico_etapa_1
        pico_stdlib
        hardware_spi
        pico_cyw43_arch_lwip_threadsafe_background
        pico_multicore
        pico_lwip_mqtt
        hardware_i2c
        FatFs_SPI)

# Add the standard include files to the build
target_include_directories(projeto_pratico_etapa_1 PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/WIFI_
        ${CMAKE_CURRENT_LIST_DIR}/OLED_
)

pico_add_extra_outputs(projeto_pratico_etapa_1)

