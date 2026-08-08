CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME filedropoverlay-demo
    TITLE "File Drop Overlay"
    DESCRIPTION "Telegram-style drag-and-drop overlay with send-as-documents and send-as-images panels."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
