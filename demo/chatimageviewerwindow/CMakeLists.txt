CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME chatimageviewerwindow-demo
    TITLE "Chat Image Viewer Window"
    DESCRIPTION "Top-level fullscreen/windowed host for ChatImageViewer with Esc and click-to-close."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/chatimageviewerwindowdemo.qrc
)
