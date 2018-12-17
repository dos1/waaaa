set(EXECUTABLE_SRC_LIST "main.c")

include(libsuperderpy-src)

target_link_libraries("libsuperderpy-${LIBSUPERDERPY_GAMENAME}" ${FFTW_LIBRARIES})
