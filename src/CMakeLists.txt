set(EXECUTABLE_SRC_LIST "main.c")
set(SHARED_SRC_LIST "common.c")

include(libsuperderpy-src)

if (FFTW_FOUND)
   target_link_libraries("lib${LIBSUPERDERPY_GAMENAME}" ${FFTW_LIBRARIES})
else (FFTW_FOUND)
   target_link_libraries("lib${LIBSUPERDERPY_GAMENAME}" fftw3)
endif (FFTW_FOUND)
