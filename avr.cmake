if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "")
    message(FATAL_ERROR "Set variable CMAKE_SYSTEM_PROCESSOR")
else()
    message(STATUS "Arch ${CMAKE_SYSTEM_PROCESSOR}")
    if ("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "atmega32")
         # 38.4k is the highest setting allowed for Arduino according to avr-libc
         # Higher settings works somewhat (like 115.2k but then the reception has errors
        add_definitions(-DF_CPU=16000000L -DBAUD=9600L)
    else()
        message(FATAL_ERROR "Unknown Atmel chip!")
    endif()
endif()


# this one is important
set(CMAKE_SYSTEM_NAME GNU) # This must be the name of one the files usr/share/cmake-3.4/Modules/Platform
#this one not so much
set(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
set(CMAKE_C_COMPILER avr-gcc)
set(CMAKE_CXX_COMPILER avr-g++)


#  -funsigned-char -funsigned-bitfields -fpack-struct -fshort-enums
set(AVR_C_FLAGS "-mmcu=${CMAKE_SYSTEM_PROCESSOR} -g -Os -std=gnu99 -Wall -Wextra -ffunction-sections -fdata-sections -mcall-prologues")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${AVR_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${AVR_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics")


# where is the target environment
set(CMAKE_FIND_ROOT_PATH /usr/lib/avr)

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
