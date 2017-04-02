cmake_minimum_required(VERSION 2.8.9) # RPI 1 Model B (wheezy Raspian has this version)


project(rf24)



set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.2), libstdc++6 (>= 4.6)")
set(CPACK_DEBIAN_PACKAGE_SECTION misc)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RF24 sensor network")
set(RF24_VERSION_MAJOR 1)
<<<<<<< HEAD
set(RF24_VERSION_MINOR 0)
=======
set(RF24_VERSION_MINOR 1)
>>>>>>> r2
set(RF24_VERSION_PATCH 0)
set(RF24_VERSION_TAG "")

set(CPACK_PACKAGE_CONTACT "Jean Gressmann <jean@0x42.de>")
set(CPACK_PACKAGE_VENDOR "Jean Gressmann")
set(CPACK_PACKAGE_VERSION_MAJOR "${RF24_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${RF24_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${RF24_VERSION_PATCH}${RF24_VERSION_TAG}")

set(CPACK_GENERATOR "DEB")


add_definitions(-D_GNU_SOURCE -DBATMAN_DEBUG -DTIME_DEBUG -DTCP_DEBUG -DSINGLECORE)
# RF24 library failure handling
add_definitions(-DFAILURE_HANDLING)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pthread -std=c99 -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread  -std=c++0x -Wall")

# RF24 library
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/RF24/utility/RPi/includes.h ${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/RF24/utility/includes.h COPYONLY)
add_library(rf24 STATIC
    3rd-party/RF24/RF24.cpp
    3rd-party/RF24/utility/RPi/bcm2835.c
    #    ../RF24/utility/RPi/interrupt.c
    3rd-party/RF24/utility/RPi/spi.cpp)


include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd-party)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/toe/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rd-party/linuxapi/include)

if (ARM)
    add_definitions(-D__arm__)
endif()


set(LIB_SOURCES
    3rd-party/toe/src/cmdlopt.c
    3rd-party/toe/src/buffer.c
    3rd-party/toe/src/stack.c
    3rd-party/linuxapi/src/utility.c
    3rd-party/linuxapi/src/epoll.c)

add_library(common STATIC ${LIB_SOURCES})

set(WEATHERBUG_SOURCES
    ../Batman.c
    ../Network.c
    ../Time.c
    ../TCP.c)

add_library(weatherbug STATIC ${WEATHERBUG_SOURCES})

add_executable(rf24-cli
    rf24-cli.cpp)
target_link_libraries(rf24-cli common readline)

add_executable(rf24-packet-router
    rf24-packet-router.cpp)
target_link_libraries(rf24-packet-router common rf24)

add_executable(rf24-network
    rf24-network.cpp)
target_link_libraries(rf24-network rt common weatherbug)

add_executable(rf24-tcp
    rf24-tcp.cpp)
target_link_libraries(rf24-tcp common weatherbug)

add_executable(rf24-ping
    rf24-ping.cpp)
target_link_libraries(rf24-ping common weatherbug)

add_executable(rf24-echo
    rf24-echo.cpp)
target_link_libraries(rf24-echo common weatherbug)

set(INSTALL_TARGETS
    rf24-echo
    rf24-ping
    rf24-tcp
    rf24-network
    rf24-packet-router)

install(TARGETS ${INSTALL_TARGETS}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib)


include(${CMAKE_ROOT}/Modules/CPack.cmake)
