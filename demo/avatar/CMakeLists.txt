CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME avatar-demo
    TITLE "Avatar"
    DESCRIPTION "Round avatars built from an image or initials, with an online/offline status badge."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/avatardemo.qrc
)
