CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT (messageformatting-test LANGUAGES CXX)

SET (HEADERS
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/testmarkdownrenderer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testsyntaxhighlighter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testmessageeditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testchatmessagetext.cpp
)

INCLUDE (../inc/test.inc.cmake)
