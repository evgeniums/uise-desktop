CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME imageeditor-demo
    TITLE "Image Editor"
    DESCRIPTION "Simple image editor with crop and freehand drawing tools."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
