CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME chatmessagefiles-demo
    TITLE "Chat Message Files / Images"
    DESCRIPTION "File and image attachments rendered inside a chat message bubble."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/chatmessagefilesdemo.qrc
)
