register_gamestate("loading")
register_gamestate("dosowisko")
register_gamestate("empty")
register_gamestate("waaaa")

target_link_libraries("libsuperderpy-${LIBSUPERDERPY_GAMENAME}-empty" ${FFTW_LIBRARIES})
target_link_libraries("libsuperderpy-${LIBSUPERDERPY_GAMENAME}-waaaa" ${FFTW_LIBRARIES})

