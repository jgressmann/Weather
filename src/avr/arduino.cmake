if("${ARDUINO_DIR}" STREQUAL "")
    if("$ENV{ARDUINO_DIR}" STREQUAL "")
        message(FATAL_ERROR "Set (environment) variable ARDUINO_DIR to the base directory of your Arduino IDE installation")
    else()
        message(STATUS "Found Arduino installation in environment variable ARDUINO_DIR: $ENV{ARDUINO_DIR}")
        set(ARDUINO_DIR "$ENV{ARDUINO_DIR}")
    endif()
endif()

string(REPLACE "\\" "/" ARDUINO_DIR "${ARDUINO_DIR}")
message(STATUS "Using Arduino installation at ${ARDUINO_DIR}")


function(GetArduinoVersion outvar)
    set(${outvar} "" PARENT_SCOPE)

    string(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+" TEMP "${ARDUINO_DIR}")

    if(NOT ("${TEMP}" STREQUAL ""))
        set(${outvar} ${TEMP} PARENT_SCOPE)
    endif()
    unset(${TEMP})
endfunction()

GetArduinoVersion(ARDUINO_VERSION)
message(STATUS "Arduino version ${ARDUINO_VERSION}")


set(AVRPATH "${ARDUINO_DIR}/hardware/tools/avr")


# avrdude extra options for Windows
if(WIN32)
    set(AVRDUDE_OPTIONS "-C \"${ARDUINO_DIR}\\hardware\\tools\\avr\\etc\\avrdude.conf\"")
    set(AVRWIN32 1)
else()
    set(AVRDUDE_OPTIONS -C "${ARDUINO_DIR}/hardware/tools/avr/etc/avrdude.conf")
endif()


include(${CMAKE_CURRENT_LIST_DIR}/avr.cmake)

function(CustomArduinoTarget)
    set(options )
    set(oneValueArgs TARGET MCU F_CPU BAUD)
    set(multiValueArgs ARDUINO_DEFINES ARDUINO_INCLUDES)
    cmake_parse_arguments(ARDUINO_CUSTOM_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    #message(STATUS "ARGN ${ARGN}")

    set(ARDUINO_INCLUDES ${ARDUINO_CUSTOM_TARGET_ARDUINO_INCLUDES})
    set(ARDUINO_INCLUDES
        ${ARDUINO_INCLUDES}
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino)

    if ("${ARDUINO_VERSION}" STREQUAL "1.6.5")
        set(ARDUINO_INCLUDES
            ${ARDUINO_INCLUDES}
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SoftwareSerial
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/utility
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SPI)


        set(ARDUINO_SOURCES
            ${ARDUINO_SOURCES}
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SoftwareSerial/SoftwareSerial.cpp
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/Wire.cpp
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/utility/twi.c
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SPI/SPI.cpp
        )
        if(NOT WIN32)
            set(ARDUINO_SOURCES
                ${ARDUINO_SOURCES}
                ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/HID.cpp)
        endif()
    else()
        set(ARDUINO_INCLUDES
            ${ARDUINO_INCLUDES}
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SoftwareSerial/src
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/src
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/utility
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SPI/src)


        set(ARDUINO_SOURCES
            ${ARDUINO_SOURCES}
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SoftwareSerial/src/SoftwareSerial.cpp
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/src/Wire.cpp
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/Wire/src/utility/twi.c
            ${ARDUINO_DIR}/hardware/arduino/avr/libraries/SPI/src/SPI.cpp
        )
    endif()
    set(ARDUINO_SOURCES
        ${ARDUINO_SOURCES}
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/WString.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/USBCore.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/HardwareSerial0.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/HardwareSerial.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/CDC.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/WMath.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/HardwareSerial2.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/HardwareSerial3.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/new.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/Tone.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/Stream.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/IPAddress.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/Print.cpp
        #${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/main.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/abi.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/HardwareSerial1.cpp
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/WInterrupts.c
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/hooks.c
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/wiring_shift.c
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/wiring_pulse.c
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/wiring_pulse.S
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/wiring_analog.c
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/wiring.c
        ${ARDUINO_DIR}/hardware/arduino/avr/cores/arduino/wiring_digital.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/COMPONENTS/MEMORY/DATA_FLASH/AT45DBX/at45dbx.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/COMPONENTS/MEMORY/DATA_FLASH/AT45DBX/at45dbx_mem.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/BOARDS/EVK1105/led.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/BOARDS/ARDUINO/led.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/PM/pm_conf_clocks.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/PM/pm.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/PM/power_clocks_lib.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/USART/usart.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC/flashc.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO/gpio.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/INTC/intc.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/DRIVERS/SPI/spi.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/SERVICES/MEMORY/CTRL_ACCESS/ctrl_access.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/UTILS/DEBUG/print_funcs.c
        #        ./firmwares/wifishield/wifi_dnld/src/SOFTWARE_FRAMEWORK/UTILS/DEBUG/debug.c
        #        ./firmwares/wifishield/wifi_dnld/src/printf-stdarg.c
        #        ./firmwares/wifishield/wifi_dnld/src/nor_flash.c
        #        ./firmwares/wifishield/wifi_dnld/src/startup.c
        #        ./firmwares/wifishield/wifi_dnld/src/flash_fw.c
        #        ./firmwares/wifishield/wifi_dnld/src/clocks.c
        #        ./firmwares/wifishield/wifiHD/src/ard_tcp.c
        #        ./firmwares/wifishield/wifiHD/src/ard_spi.c
        #        ./firmwares/wifishield/wifiHD/src/main.c
        #        ./firmwares/wifishield/wifiHD/src/util.c
        #        ./firmwares/wifishield/wifiHD/src/owl_os.c
        #        ./firmwares/wifishield/wifiHD/src/fw_download_extflash.c
        #        ./firmwares/wifishield/wifiHD/src/timer.c
        #        ./firmwares/wifishield/wifiHD/src/avr32_spi.c
        #        ./firmwares/wifishield/wifiHD/src/console.c
        #        ./firmwares/wifishield/wifiHD/src/ping.c
        #        ./firmwares/wifishield/wifiHD/src/lwip_setup.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/COMPONENTS/MEMORY/DATA_FLASH/AT45DBX/at45dbx.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/COMPONENTS/MEMORY/DATA_FLASH/AT45DBX/at45dbx_mem.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/BOARDS/EVK1105/led.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/BOARDS/ARDUINO/led.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/RTC/rtc.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/PM/pm_conf_clocks.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/PM/pm.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/PM/power_clocks_lib.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/USART/usart.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/TC/tc.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/FLASHC/flashc.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/EBI/SMC/smc.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/GPIO/gpio.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/INTC/intc.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/SPI/spi.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/PDCA/pdca.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/DRIVERS/EIC/eic.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/MEMORY/CTRL_ACCESS/ctrl_access.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/init.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/udp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/raw.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/inet.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/autoip.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/ip.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/igmp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/inet_chksum.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/ip_frag.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/ip_addr.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/ipv4/icmp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/tcp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/tcp_out.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/stats.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/dhcp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/memp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/tcp_in.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/netif.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/dns.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/pbuf.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/core/mem.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/netif/loopif.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-1.3.2/src/netif/etharp.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/LWIP/lwip-port-1.3.2/HD/if/netif/wlif.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/SERVICES/DELAY/delay.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/UTILS/DEBUG/print_funcs.c
        #        ./firmwares/wifishield/wifiHD/src/SOFTWARE_FRAMEWORK/UTILS/DEBUG/debug.c
        #        ./firmwares/wifishield/wifiHD/src/printf-stdarg.c
        #        ./firmwares/wifishield/wifiHD/src/ard_utils.c
        #        ./firmwares/wifishield/wifiHD/src/wl_cm.c
        #        ./firmwares/wifishield/wifiHD/src/board_init.c
        #        ./firmwares/wifishield/wifiHD/src/cmd_wl.c
        #        ./firmwares/wifishield/wifiHD/src/nvram.c
        #        ./firmwares/atmegaxxu2/arduino-usbdfu/Arduino-usbdfu.c
        #        ./firmwares/atmegaxxu2/arduino-usbdfu/Descriptors.c
        #        ./firmwares/atmegaxxu2/arduino-usbserial/Arduino-usbserial.c
        #        ./firmwares/atmegaxxu2/arduino-usbserial/Descriptors.c
        #        ./bootloaders/atmega/ATmegaBOOT_168.c
        #        ./bootloaders/bt/ATmegaBOOT_168.c
        #        ./bootloaders/caterina-LilyPadUSB/Caterina.c
        #        ./bootloaders/caterina-LilyPadUSB/Descriptors.c
        #        ./bootloaders/lilypad/src/ATmegaBOOT.c
        #        ./bootloaders/caterina/Caterina.c
        #        ./bootloaders/caterina/Descriptors.c
        #        ./bootloaders/caterina-Arduino_Robot/Caterina.c
        #        ./bootloaders/caterina-Arduino_Robot/Descriptors.c
        #        ./bootloaders/stk500v2/stk500boot.c
        #        ./bootloaders/atmega8/ATmegaBOOT.c
        #        ./bootloaders/optiboot/optiboot.c
        )
    add_library(${ARDUINO_CUSTOM_TARGET_TARGET}-arduino STATIC ${ARDUINO_SOURCES})
    get_property(CF TARGET ${ARDUINO_CUSTOM_TARGET_TARGET}-arduino PROPERTY COMPILE_FLAGS)
    set_target_properties(${ARDUINO_CUSTOM_TARGET_TARGET}-arduino PROPERTIES COMPILE_FLAGS "${CF} -w -mmcu=${ARDUINO_CUSTOM_TARGET_MCU} -DF_CPU=${ARDUINO_CUSTOM_TARGET_F_CPU}L -DBAUD==${ARDUINO_CUSTOM_TARGET_BAUD}L") # turn off warnings for Arduino code
    target_compile_definitions(${ARDUINO_CUSTOM_TARGET_TARGET}-arduino PRIVATE ${ARDUINO_CUSTOM_TARGET_ARDUINO_DEFINES})
    target_include_directories(${ARDUINO_CUSTOM_TARGET_TARGET}-arduino PRIVATE ${ARDUINO_INCLUDES})

    AvrTarget(TARGET ${ARDUINO_CUSTOM_TARGET_TARGET} MCU ${ARDUINO_CUSTOM_TARGET_MCU} F_CPU ${ARDUINO_CUSTOM_TARGET_F_CPU} BAUD ${ARDUINO_CUSTOM_TARGET_BAUD} ${ARDUINO_CUSTOM_TARGET_UNPARSED_ARGUMENTS})
    target_link_libraries(${ARDUINO_CUSTOM_TARGET_TARGET}.elf ${ARDUINO_CUSTOM_TARGET_TARGET}-arduino)
    target_compile_definitions(${ARDUINO_CUSTOM_TARGET_TARGET}.elf PRIVATE ${ARDUINO_CUSTOM_TARGET_ARDUINO_DEFINES})
    target_include_directories(${ARDUINO_CUSTOM_TARGET_TARGET}.elf PRIVATE ${ARDUINO_INCLUDES})

endfunction()


function(ArduinoTarget)
    set(options )
    set(oneValueArgs TARGET BOARD)
    set(multiValueArgs )
    cmake_parse_arguments(ARDUINO_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if("${ARDUINO_TARGET_BOARD}" STREQUAL "")
        message(FATAL_ERROR "Set the BOARD variable to e.g. 'Arduino Uno'")
    endif()

    if ("${ARDUINO_TARGET_TARGET}" STREQUAL "")
        message(FATAL_ERROR "Set TARGET variable")
    endif()

    string(TOLOWER "${ARDUINO_TARGET_BOARD}" lowerBoard)
    if("${lowerBoard}" MATCHES "arduino")
        set(MCU atmega328p)
        if ("${lowerBoard}" MATCHES "uno")
            set(ARDUINO_DEFINES -DARDUINO=10605 -DARDUINO_AVR_UNO -DARDUINO_ARCH_AVR)
            set(ARDUINO_INCLUDES ${ARDUINO_DIR}/hardware/arduino/avr/variants/standard)
            set(PROGBAUD 115200)
            set(BAUD 115200)
            set(F_CPU 16000000)
        elseif("${lowerBoard}" MATCHES "nano")
            set(ARDUINO_DEFINES -DARDUINO=10607 -DARDUINO_AVR_NANO -DARDUINO_ARCH_AVR)
            set(ARDUINO_INCLUDES ${ARDUINO_DIR}/hardware/arduino/avr/variants/eightanaloginputs)
            set(PROGBAUD 57600)
            set(BAUD 57600)
            set(F_CPU 16000000)
        else()
            message(FATAL_ERROR "Unknown Arduino board '${ARDUINO_TARGET_BOARD}'")
        endif()
    else()
        message(FATAL_ERROR "Unknown board '${ARDUINO_TARGET_BOARD}'")
    endif()

    CustomArduinoTarget(
        TARGET ${ARDUINO_TARGET_TARGET}
        ARDUINO_DEFINES ${ARDUINO_DEFINES}
        ARDUINO_INCLUDES ${ARDUINO_INCLUDES}
        MCU ${MCU} F_CPU ${F_CPU} BAUD ${BAUD} ${ARDUINO_TARGET_UNPARSED_ARGUMENTS})

    #-U eeprom:w:${targetName}.eep.hex:i
    set(AVRDUDE_CMDLINE ${AVRPATH}/bin/avrdude ${AVRDUDE_OPTIONS} -v -p ${MCU} -c arduino -b ${PROGBAUD} -U flash:w:${ARDUINO_TARGET_TARGET}.hex:i)
    set(AVRDUDE_CMDLINE_CMAKE_PORT "${AVRDUDE_CMDLINE} -P ${PORT}")
    #message(STATUS "${CMAKE_SYSTEM_NAME}")
    if(AVRWIN32)
        set(AVRDUDE_CMDLINE_ENV_PORT ${AVRDUDE_CMDLINE} -P "$(PORT)") # NMAKE syntax
    else()
        if ("${CMAKE_GENERATOR}" MATCHES "Unix Makefiles")
            set(AVRDUDE_CMDLINE_ENV_PORT ${AVRDUDE_CMDLINE} -P $$PORT)
        else()
            set(AVRDUDE_CMDLINE_ENV_PORT ${AVRDUDE_CMDLINE} -P $PORT)
        endif()
    endif()
    # flash_XXX_to_port, works with any serial port (use env var PORT)
    add_custom_target(
        flashx_${ARDUINO_TARGET_TARGET}
        COMMAND ${AVRDUDE_CMDLINE_ENV_PORT}
        DEPENDS ${ARDUINO_TARGET_TARGET}.hex ${ARDUINO_TARGET_TARGET}.eep.hex)

    set(FLASH_TARGET_NAME flash_${ARDUINO_TARGET_TARGET})
    if (NOT "${PORT}" STREQUAL "")
        message(STATUS "Variable PORT set, adding target ${FLASH_TARGET_NAME} using ${PORT}")
        add_custom_target(
            flash_${ARDUINO_TARGET_TARGET}
            COMMAND ${AVRDUDE_CMDLINE_CMAKE_PORT}
            #-U eeprom:w:${targetName}.eep.hex:i
            DEPENDS ${targetName}.hex ${targetName}.eep.hex)
    else()
        message(STATUS "Variable PORT not set; target ${FLASH_TARGET_NAME} won't be generated")
    endif()
endfunction()


