CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT (messageformatting-test LANGUAGES CXX)

SET (HEADERS
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/testmarkdownrenderer.cpp
)

INCLUDE (../inc/test.inc.cmake)
