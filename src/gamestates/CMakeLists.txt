include(libsuperderpy-gamestates)

if (NOT FFTW_FOUND)
   target_link_libraries("lib${LIBSUPERDERPY_GAMENAME}-waaaa" fftw3)
endif (NOT FFTW_FOUND)
