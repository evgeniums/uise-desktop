CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME editbar-demo
    TITLE "Edit message UI"
    DESCRIPTION "Edit-preview bar shown above the chat editor while editing an existing message, reusing the same preview block as the reply/forward bars."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
