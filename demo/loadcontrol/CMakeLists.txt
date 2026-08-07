CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME loadcontrol-demo
    TITLE "Load Control"
    DESCRIPTION "Load control widget paired with a toast notification."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
