CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME messageformatting-demo
    TITLE "Message Formatting Foundations"
    DESCRIPTION "Document-level CSS applied to chat message text, and the syntax-colour theme JSON, both reloading live across Auto/Light/Dark."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
