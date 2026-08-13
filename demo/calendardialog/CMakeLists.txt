CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME calendardialog-demo
    TITLE "Calendar Dialog"
    DESCRIPTION "Floating calendar dialog that closes itself after a period of inactivity."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
