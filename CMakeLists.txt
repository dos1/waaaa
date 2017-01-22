cmake_minimum_required(VERSION 2.8)

project(waaaa C)

SET(LIBSUPERDERPY_GAMENAME "waaaa" CACHE INTERNAL "")
SET(LIBSUPERDERPY_GAMENAME_PRETTY "WAAAA" CACHE INTERNAL "")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/libsuperderpy/cmake")

add_definitions(-DALLEGRO_UNSTABLE=1)

include(libsuperderpy)
include(SetPaths)

find_package(FFTW REQUIRED)

add_subdirectory(libsuperderpy)
add_subdirectory(src)
add_subdirectory(data)

# uninstall target
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/libsuperderpy/cmake/cmake_uninstall.cmake.in" "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake" IMMEDIATE @ONLY)
add_custom_target(uninstall COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
