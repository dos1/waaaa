cmake_minimum_required(VERSION 3.6...3.27)

project(waaaa C)

SET(LIBSUPERDERPY_GAMENAME "waaaa" CACHE INTERNAL "")
SET(LIBSUPERDERPY_GAMENAME_PRETTY "WAAAA" CACHE INTERNAL "")

SET(LIBSUPERDERPY_APPID "net.dosowisko.WAAAA")
SET(LIBSUPERDERPY_ORIENTATION "LANDSCAPE")
SET(LIBSUPERDERPY_VERSION "1.0")
SET(LIBSUPERDERPY_VITA_TITLEID "DOS1WAAAA" CACHE INTERNAL "")

set(EMSCRIPTEN_TOTAL_MEMORY "128" CACHE INTERNAL "")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" "${CMAKE_SOURCE_DIR}/libsuperderpy/cmake")

option(BUNDLED_FFTW "Use bundled FFTW even if system-wide one is available" OFF)

if (NOT BUNDLED_FFTW)
  find_package(FFTW)
endif (NOT BUNDLED_FFTW)

if (NOT FFTW_FOUND)
  message(STATUS "Using bundled FFTW")
  add_subdirectory(3rdparty)
  include_directories("3rdparty/fftw/api")
endif (NOT FFTW_FOUND)

include(libsuperderpy)

add_subdirectory(libsuperderpy)
add_subdirectory(src)
add_subdirectory(data)
