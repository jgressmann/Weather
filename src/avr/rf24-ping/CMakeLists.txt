cmake_minimum_required(VERSION 2.8.11)

include(../avr.cmake)

project(rf24-ping)



#add_definitions(-DNDEBUG)


set(SOURCES
    ../USART0.c
    ../USART0.h
    ../SPI.c
    ../SPI.h
    ../RF24.c
    ../RF24.h
    main.cpp)



if(NOT F_CPU)
    set(F_CPU 8000000) # default for HWRV 1.2
endif()
AvrTarget(
    TARGET ${PROJECT_NAME}
    MCU atmega328p
    F_CPU ${F_CPU}
    BAUD 38400
    PRINTFMIN
    ${SOURCES})




add_custom_target(
    flash_r1.2
    # this is for internal RC osc. @ 8Mhz
    #COMMAND avrdude $$AVRDUDE_OPTIONS -v -p atmega328p -c usbasp -U flash:w:${PROJECT_NAME}.hex:i -U lfuse:w:0xe2:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m
    # this is for external full swing crystal @ 8+Mhz
    COMMAND avrdude $$AVRDUDE_OPTIONS -v -p atmega328p -c usbasp -U flash:w:${PROJECT_NAME}.hex:i -U lfuse:w:0xf7:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m
    DEPENDS ${PROJECT_NAME}.hex ${PROJECT_NAME}.eep.hex)
