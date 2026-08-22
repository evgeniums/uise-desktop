CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME forwardpreview-demo
    TITLE "Forwarding UI"
    DESCRIPTION "Message forwarding widgets: forward bar, bubble header/comment sections and the full-preview dialog."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
