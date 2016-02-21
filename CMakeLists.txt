cmake_minimum_required(VERSION 2.8.11)

set(CMAKE_SYSTEM_PROCESSOR atmega328p)
include(avr.cmake)

project(weatherbug)


#set(CMAKE_VERBOSE_MAKEFILE 1)

function(AvrTarget targetName)
    # Compile and link
    add_executable(${targetName}.elf ${ARGN})
    get_property(LF TARGET ${targetName}.elf PROPERTY LINK_FLAGS)
    set_target_properties(${targetName}.elf PROPERTIES LINK_FLAGS "-Wl,-Map,${targetName}.map -Wl,--gc-sections ${LF}")

    # eeprom file
    add_custom_target(
        ${targetName}.eep.hex
        #COMMAND avr-objcopy -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 ${targetName}.elf ${targetName}.eep
        COMMAND avr-objcopy -j .eeprom --change-section-lma .eeprom=0 -O ihex ${targetName}.elf ${targetName}.eep.hex
        DEPENDS ${targetName}.elf)

#    # file for avrdude
#    add_custom_target(
#        ${targetName}.hex
#        COMMAND avr-objcopy -O ihex -R .eeprom ${targetName}.elf ${targetName}.hex
#        DEPENDS ${targetName}.eep)

    # Hex file for avrdude
    add_custom_target(
        ${targetName}.hex
        COMMAND avr-objcopy -j .text -j .data -O ihex ${targetName}.elf ${targetName}.hex
        DEPENDS ${targetName}.elf)

    # List file
    add_custom_target(
        ${targetName}.lst
        COMMAND avr-objdump -h -S ${targetName}.elf > ${targetName}.lst
        DEPENDS ${targetName}.elf)

    # bind targets
    add_custom_target(${targetName} ALL DEPENDS
        ${targetName}.elf
        ${targetName}.eep.hex
        ${targetName}.hex
        ${targetName}.lst)

    set(FLASH_TARGET_NAME flash_${targetName})
    if (NOT "${PORT}" STREQUAL "")
        message(STATUS "Variable PORT set, adding target ${FLASH_TARGET_NAME} using ${PORT}")
        add_custom_target(
            flash_${targetName}
            COMMAND avrdude -v -p ${CMAKE_SYSTEM_PROCESSOR} -carduino -P ${PORT} -b115200 -U flash:w:${targetName}.hex:i -U eeprom:w:${targetName}.eep.hex:i
            DEPENDS ${targetName}.hex ${targetName}.eep.hex)
    else()
        message(STATUS "Variable PORT not set; target ${FLASH_TARGET_NAME} won't be generated")
    endif()
endfunction()

set(SOURCES
    Watchdog.c
    Watchdog.h
    USART0.c
    USART0.h
    Debug.h
    main.c)

AvrTarget(${PROJECT_NAME} ${SOURCES})
