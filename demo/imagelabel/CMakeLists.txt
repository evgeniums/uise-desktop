CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME imagelabel-demo
    TITLE "Image Label"
    DESCRIPTION "Image label widget, including animated GIF playback."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/imagelabeldemo.qrc
)
