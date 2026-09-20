CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME audioplayer-demo
    TITLE "Audio Player"
    DESCRIPTION "Generic audio player, panel and floating dialog, with voice message and audio file rows."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
