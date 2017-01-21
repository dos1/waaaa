register_gamestate("loading")
register_gamestate("dosowisko")
register_gamestate("empty")

target_link_libraries("libsuperderpy-${LIBSUPERDERPY_GAMENAME}-empty" ${FFTW_LIBRARIES})
