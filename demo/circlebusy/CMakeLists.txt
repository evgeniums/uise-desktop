CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME circlebusy-demo
    TITLE "Circle Busy"
    DESCRIPTION "Circular progress indicator combined with a toast notification."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
