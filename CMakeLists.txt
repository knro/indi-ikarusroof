cmake_minimum_required(VERSION 2.8)
PROJECT(indi_ikarusroof CXX C)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake_modules/")
set(BIN_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/bin")
set(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib")

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

find_package(INDI REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Nova REQUIRED)
find_package(CURL REQUIRED)

set(INDI_IKARUSROOF_VERSION_MAJOR 0)
set(INDI_IKARUSROOF_VERSION_MINOR 1)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

include_directories( ${CMAKE_CURRENT_BINARY_DIR})
include_directories( ${CMAKE_CURRENT_SOURCE_DIR})
include_directories( ${INDI_INCLUDE_DIR})
include_directories(${NOVA_INCLUDE_DIR})

########### Ikarus Roof ###########
set(indi_ikarusroof_SRCS
   ${CMAKE_CURRENT_SOURCE_DIR}/ikarus_roof.cpp
   )

add_executable(indi_ikarusroof_dome ${indi_ikarusroof_SRCS})

target_link_libraries(indi_ikarusroof_dome ${INDI_DRIVER_LIBRARIES} ${CURL_LIBRARIES} -lwiringPi)

install(TARGETS indi_ikarusroof_dome RUNTIME DESTINATION bin)
install(FILES indi_ikarusroof.xml DESTINATION ${INDI_DATA_DIR})

