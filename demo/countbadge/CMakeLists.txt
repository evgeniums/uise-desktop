CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME countbadge-demo
    TITLE "Count Badge"
    DESCRIPTION "Circle-to-pill counter badge used for unread message counts."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
