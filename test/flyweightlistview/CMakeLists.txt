CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT (flyweightlistview-test LANGUAGES CXX)

SET (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/helloworlditem.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fwlvtestwidget.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/fwlvtestcontext.hpp
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/fwlvtestwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testfwlvsettersgetters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testfwlvloaditems.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testfwlvresize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testfwlvscroll.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testfwlvjump.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/testfwlvinsertdelete.cpp
)

INCLUDE (../inc/test.inc.cmake)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
