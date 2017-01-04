cmake_minimum_required(VERSION 2.8.11)

include(../arduino.cmake)
project(rf24-echo)

add_definitions(-DRF24_DEBUG)


set(SOURCES
    ../USART0.c
    ../USART0.h
    ../SPI.c
    ../SPI.h
    ../RF24.c
    ../RF24.h
    main.cpp)


# PORT=/dev/ttyACM0 make flashx_analogin-arduino && picocom -b115200 --echo  --imap lfcrlf --omap ignlf,crlf  /dev/ttyACM0
ArduinoTarget(TARGET ${PROJECT_NAME}-arduino BOARD "Arduino Uno" ${SOURCES})


if(ARDUINO)
    if(UNO)
        AvrTarget(
            TARGET ${PROJECT_NAME}
            MCU atmega328p
            F_CPU 16000000
            BAUD 115200
            PRINTFMIN
            ${SOURCES})
    else()
        AvrTarget(
            TARGET ${PROJECT_NAME}
            MCU atmega328p
            F_CPU 16000000
            BAUD 57600
            PRINTFMIN
            ${SOURCES})
    endif()
else()
    AvrTarget(
        TARGET ${PROJECT_NAME}
        MCU atmega328p
        F_CPU 8000000
        BAUD 38400
        PRINTFMIN
        ${SOURCES})
endif()


add_custom_target(
    flash_nano
    COMMAND avrdude $$AVRDUDE_OPTIONS -v -P $$PORT -p atmega328p -c arduino -b 57600 -U flash:w:${PROJECT_NAME}.hex:i
    DEPENDS ${PROJECT_NAME}.hex ${PROJECT_NAME}.eep.hex)

# PORT=/dev/ttyACM0 make flash_uno && picocom -b115200 --echo  --imap lfcrlf --omap ignlf,crlf  /dev/ttyACM0
add_custom_target(
    flash_uno
    COMMAND avrdude $$AVRDUDE_OPTIONS -v -P $$PORT -p atmega328p -c arduino -b 115200 -U flash:w:${PROJECT_NAME}.hex:i
    DEPENDS ${PROJECT_NAME}.hex ${PROJECT_NAME}.eep.hex)

add_custom_target(
    flash_r1.2
    # this is for internal RC osc. @ 8Mhz
    #COMMAND avrdude $$AVRDUDE_OPTIONS -v -p atmega328p -c usbasp -U flash:w:${PROJECT_NAME}.hex:i -U lfuse:w:0xe2:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m
    # this is for external full swing crystal @ 8+Mhz
    COMMAND avrdude $$AVRDUDE_OPTIONS -v -p atmega328p -c usbasp -U flash:w:${PROJECT_NAME}.hex:i -U lfuse:w:0xf7:m -U hfuse:w:0xd6:m -U efuse:w:0xff:m
    DEPENDS ${PROJECT_NAME}.hex ${PROJECT_NAME}.eep.hex)
