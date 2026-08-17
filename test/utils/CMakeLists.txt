CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT (utils-test LANGUAGES CXX)

SET (HEADERS
)

SET (SOURCES
    testorientationinvariant.cpp
    testmiscutils.cpp
    testalbumlayout.cpp
)

INCLUDE (../inc/test.inc.cmake)
