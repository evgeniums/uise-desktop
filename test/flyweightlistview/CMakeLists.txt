CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT (flyweightlistview-test LANGUAGES CXX)

SET (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../demo/flyweightlistview/helloworlditem.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fwlvtestwidget.hpp
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fwlvtestwidget.cpp
)

INCLUDE (../inc/test.inc.cmake)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../demo/flyweightlistview)
