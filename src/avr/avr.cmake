
# CMake style include guard
if(AVR_CMAKE_INCLUDED)
  return()
endif()
set(AVR_CMAKE_INCLUDED 1)


include(CMakeParseArguments)

# Path to comilers, headers, libs
if("${AVRPATH}" STREQUAL "")
    message(STATUS "AVRPATH not set, using /usr")
    set(AVRPATH /usr)
else()
    message(STATUS "Using AVRPATH ${AVRPATH}")
    if(WIN32)
        string(REPLACE "\\" "/" AVRPATH "${AVRPATH}")
    endif()
endif()


include (CMakeForceCompiler)

# This must be the name of one the files usr/share/cmake-3.4/Modules/Platform
# Ideally set this to a system that is known to use GCC so that compiler/linker
# errors are picked up by QtCreator
set(CMAKE_SYSTEM_NAME AVR)

# Not sure what this is for
set(CMAKE_SYSTEM_VERSION 1)

# specify the cross compiler
if(WIN32)
    set(EXE .exe)
endif()
CMAKE_FORCE_C_COMPILER(${AVRPATH}/bin/avr-gcc${EXE} AvrGcc)
CMAKE_FORCE_CXX_COMPILER(${AVRPATH}/bin/avr-g++${EXE} AvrGxx)

#  -funsigned-char -funsigned-bitfields
set(AVR_C_FLAGS "-g -Os -Wall -Wextra -ffunction-sections -fdata-sections -mcall-prologues -fpack-struct -fshort-enums ${AVR_C_FLAGS}")
set(AVR_CXX_FLAGS "${AVR_C_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics ${AVR_CXX_FLAGS}")
set(CMAKE_C_FLAGS " -std=gnu99 ${AVR_C_FLAGS}" CACHE "AVR C flags" STRING FORCE)
set(CMAKE_CXX_FLAGS "-std=gnu++11 ${AVR_CXX_FLAGS}" CACHE STRING "AVR CXX flags" FORCE)

## So this is new
#set(CMAKE_C_FLAGS_DEBUG ${CMAKE_C_FLAGS} CACHE "C flags" STRING FORCE)
#set(CMAKE_C_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS} CACHE "C flags" STRING FORCE)
#set(CMAKE_C_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS} CACHE "C flags" STRING FORCE)
#set(CMAKE_C_FLAGS_RELEASE ${CMAKE_C_FLAGS} CACHE "C flags" STRING FORCE)
#set(CMAKE_CXX_FLAGS_DEBUG ${CMAKE_CXX_FLAGS} CACHE "CXX flags" STRING FORCE)
#set(CMAKE_CXX_FLAGS_MINSIZEREL ${CMAKE_CXX_FLAGS} CACHE "CXX flags" STRING FORCE)
#set(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CMAKE_CXX_FLAGS} CACHE "CXX flags" STRING FORCE)
#set(CMAKE_CXX_FLAGS_RELEASE ${CMAKE_CXX_FLAGS} CACHE "CXX flags" STRING FORCE)

#set(CMAKE_EXE_LINKER_FLAGS ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_EXE_LINKER_FLAGS_DEBUG ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_EXE_LINKER_FLAGS_RELEASE ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)

#set(CMAKE_MODULE_LINKER_FLAGS ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_MODULE_LINKER_FLAGS_DEBUG ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_MODULE_LINKER_FLAGS_RELEASE ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)
#set(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS} CACHE "" STRING FORCE)



set(CMAKE_FIND_ROOT_PATH ${AVRPATH})

# search for programs in the build host directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)


function(AvrTarget)
    set(options PRINTFMIN)
    set(oneValueArgs TARGET MCU F_CPU BAUD)
    set(multiValueArgs )
    cmake_parse_arguments(AVR_TARGET "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if ("${AVR_TARGET_TARGET}" STREQUAL "")
        message(FATAL_ERROR "Set TARGET variable")
    endif()

    if ("${AVR_TARGET_MCU}" STREQUAL "")
        message(FATAL_ERROR "Set MCU variable to something avrdude understands like atmega328p")
    endif()

    if ("${AVR_TARGET_F_CPU}" STREQUAL "")
        message(FATAL_ERROR "Set F_CPU variable to something in Hz like 16000000")
    endif()

    if ("${AVR_TARGET_BAUD}" STREQUAL "")
        message(FATAL_ERROR "Set BAUD variable to something in BPS like 9600")
    endif()

    if (AVR_TARGET_PRINTFMIN)
        message(STATUS "Using minimal printf implementation")
        set(AVR_EXTRA_LINKER_FLAGS "-Wl,-u,vfprintf -lprintf_min")
    else()
        message(STATUS "Using full printf implementation")
        set(AVR_EXTRA_LINKER_FLAGS "-Wl,-u,vfprintf -lprintf_flt -lm")
    endif()


    # Compile and link
    set(ELF_TARGET ${AVR_TARGET_TARGET}.elf)
    add_executable(${ELF_TARGET} ${AVR_TARGET_UNPARSED_ARGUMENTS})
    get_property(LF TARGET ${ELF_TARGET} PROPERTY LINK_FLAGS)
    set_target_properties(${ELF_TARGET} PROPERTIES LINK_FLAGS "-Wl,-Map,${AVR_TARGET_TARGET}.map -Wl,--gc-sections -mmcu=${AVR_TARGET_MCU} ${AVR_EXTRA_LINKER_FLAGS} ${LF}")
    get_property(CF TARGET ${ELF_TARGET} PROPERTY COMPILE_FLAGS)
    # -save-temps
    set_target_properties(${ELF_TARGET} PROPERTIES COMPILE_FLAGS "-mmcu=${AVR_TARGET_MCU} ${CF}")
    target_compile_definitions(${ELF_TARGET} PRIVATE -DF_CPU=${AVR_TARGET_F_CPU}L -DBAUD=${AVR_TARGET_BAUD}L)
#    get_property(CF TARGET ${ELF_TARGET} PROPERTY CXX_FLAGS)
#    set_target_properties(${ELF_TARGET} PROPERTIES CXX_FLAGS "${AVR_EXTRA_CFLAGS} ${CF}")


    # eeprom file
    add_custom_target(
        ${AVR_TARGET_TARGET}.eep.hex
        #COMMAND avr-objcopy -O ihex -j .eeprom --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 ${ELF_TARGET} ${targetName}.eep
        COMMAND ${AVRPATH}/bin/avr-objcopy -j .eeprom --change-section-lma .eeprom=0 -O ihex ${ELF_TARGET} ${AVR_TARGET_TARGET}.eep.hex
        DEPENDS ${ELF_TARGET})

    # Hex file for avrdude
    add_custom_target(
        ${AVR_TARGET_TARGET}.hex
        COMMAND ${AVRPATH}/bin/avr-objcopy -j .text -j .data -O ihex ${ELF_TARGET} ${AVR_TARGET_TARGET}.hex
        DEPENDS ${ELF_TARGET})

    # List file
    add_custom_target(
        ${AVR_TARGET_TARGET}.lst
        COMMAND ${AVRPATH}/bin/avr-objdump -h -S ${ELF_TARGET} > ${AVR_TARGET_TARGET}.lst
        DEPENDS ${ELF_TARGET})

    # bind targets
    add_custom_target(${AVR_TARGET_TARGET} ALL DEPENDS
        ${ELF_TARGET}
        ${AVR_TARGET_TARGET}.eep.hex
        ${AVR_TARGET_TARGET}.hex
        ${AVR_TARGET_TARGET})
endfunction()

