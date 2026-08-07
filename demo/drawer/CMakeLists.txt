CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME drawer-demo
    TITLE "Drawer"
    DESCRIPTION "Sliding drawer panel that opens from the edge of the window."
    HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/helloworlditem.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/fwlvtestwidget.hpp
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/fwlvtestwidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

TARGET_INCLUDE_DIRECTORIES(drawer-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview)
