CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME chatreactions-demo
    TITLE "Chat Reactions"
    DESCRIPTION "Reaction chips on a chat message bubble, plus the reaction gallery popup."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
