CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME newpasswordwizard-demo
    TITLE "New Password Wizard"
    DESCRIPTION "Multi-step wizard dialog for setting a new password."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
