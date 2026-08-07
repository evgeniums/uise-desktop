CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME busywaiting-demo
    TITLE "Busy Waiting"
    DESCRIPTION "Busy-waiting spinner overlay shown while a frame is loading."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
