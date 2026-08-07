CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME audiodevicesetting-demo
    TITLE "Audio Device Setting"
    DESCRIPTION "Audio input/output device picker with a live input level meter."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
