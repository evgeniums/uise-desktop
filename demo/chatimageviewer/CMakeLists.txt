CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME chatimageviewer-demo
    TITLE "Chat Image Viewer"
    DESCRIPTION "Telegram-style overlay image viewer with counter, sender/datetime, album strip and toolbar."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/chatimageviewerdemo.qrc
)
