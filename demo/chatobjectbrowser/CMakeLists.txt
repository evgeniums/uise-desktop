CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME chatobjectbrowser-demo
    TITLE "Chat Object Browsers"
    DESCRIPTION "Browsers of the images, files, voice messages, links, mentions and found messages of a chat, with fake keyset paging."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
