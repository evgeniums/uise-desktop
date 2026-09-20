CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME waveformbar-demo
    TITLE "Waveform Bar"
    DESCRIPTION "Seekable, croppable audio waveform and line progress."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
