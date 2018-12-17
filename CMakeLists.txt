cmake_minimum_required(VERSION 2.8)

project(waaaa C)

SET(LIBSUPERDERPY_GAMENAME "waaaa" CACHE INTERNAL "")
SET(LIBSUPERDERPY_GAMENAME_PRETTY "WAAAA" CACHE INTERNAL "")

SET(LIBSUPERDERPY_APPID "net.dosowisko.WAAAA")
SET(LIBSUPERDERPY_ORIENTATION "LANDSCAPE")
SET(LIBSUPERDERPY_VERSION "1.0")

set(EMSCRIPTEN_TOTAL_MEMORY "128" CACHE INTERNAL "")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/libsuperderpy/cmake")

include(libsuperderpy)

find_package(FFTW REQUIRED)

add_subdirectory(libsuperderpy)
add_subdirectory(src)
add_subdirectory(data)
