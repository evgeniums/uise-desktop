CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME framewithstatus-demo
    TITLE "Frame With Modal Status"
    DESCRIPTION "Frame with a modal status overlay, including busy and skeleton states."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
