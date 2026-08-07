CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME passwordinput-demo
    TITLE "Password Input"
    DESCRIPTION "Password input field and the new-password panel with strength feedback."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
