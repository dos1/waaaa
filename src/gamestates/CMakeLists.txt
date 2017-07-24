FILE (GLOB gamestates "*.c")
FOREACH(gamestate ${gamestates})
    get_filename_component(gamestate_name ${gamestate} NAME_WE)
    register_gamestate(${gamestate_name})
ENDFOREACH(gamestate)

target_link_libraries("libsuperderpy-${LIBSUPERDERPY_GAMENAME}-empty" ${FFTW_LIBRARIES})
target_link_libraries("libsuperderpy-${LIBSUPERDERPY_GAMENAME}-waaaa" ${FFTW_LIBRARIES})
