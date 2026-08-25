CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME filedropoverlay-demo
    TITLE "File Drop Overlay"
    DESCRIPTION "Telegram-style drag-and-drop overlay, with a selectable images mode: documents+normal or full+normal panel pairs."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
