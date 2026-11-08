CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT (chatobjectbrowser-test LANGUAGES CXX)

SET (HEADERS
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/testchatobjectmodel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testchatobjectbrowser.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testchatobjectsearch.cpp
)

INCLUDE (../inc/test.inc.cmake)
