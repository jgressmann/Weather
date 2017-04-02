cmake_minimum_required(VERSION 2.8.11)

include(../avr.cmake)

project(weatherbug)


add_definitions(-DNDEBUG)
#add_definitions(-DBATMAN_DEBUG)
#add_definitions(-DTIME_DEBUG)
#add_definitions(-DTCP_DEBUG)


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")

set(SOURCES
    ${SOURCES}
    Watchdog.c
    Watchdog.h
    Timer.c
    Timer.h
    ../USART0.c
    ../USART0.h
    ../Debug.h
    ../../Misc.h
    ../DHT.c
    ../DHT.h
    ../../Batman.c
    ../../Batman.h
    ../../Network.c
    ../../Network.h
    ../../Time.c
    ../../Time.h
    ../../TCP.c
    ../../TCP.h
    ../SPI.c
    ../SPI.h
    ../RF24.c
    ../RF24.h
    main.cpp)


if(NOT F_CPU)
    set(F_CPU 8000000) # default for HWRV 1.2
endif()


# Soooo for some mpus 57600 works
AvrTarget(
    TARGET ${PROJECT_NAME}
    MCU atmega328p
    F_CPU ${F_CPU}
    BAUD 38400
    PRINTFMIN
    ${SOURCES})

add_custom_target(
    flash_r1.2
    # See http://www.engbedded.com/fusecalc
    # this is for internal RC osc. @ 8Mhz
    #COMMAND avrdude $$AVRDUDE_OPTIONS -v -p atmega328p -c usbasp -U flash:w:${PROJECT_NAME}.hex:i -U lfuse:w:0xe2:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m
    # this is for external full swing crystal @ 0.4-22 Mhz
    COMMAND avrdude $$AVRDUDE_OPTIONS -v -p atmega328p -c usbasp -U flash:w:${PROJECT_NAME}.hex:i -U lfuse:w:0xf7:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m
    DEPENDS ${PROJECT_NAME}.hex ${PROJECT_NAME}.eep.hex)

