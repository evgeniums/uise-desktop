CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME elidedcontainer-demo
    TITLE "Elided Container"
    DESCRIPTION "Container that elides its overflowing child widgets."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
