CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME flyweightlistview-demo
    TITLE "Flyweight List View"
    DESCRIPTION "Virtualized list view that recycles item widgets for large data sets."
    HEADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/helloworlditem.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/fwlvtestwidget.hpp
    SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/fwlvtestwidget.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

TARGET_INCLUDE_DIRECTORIES(flyweightlistview-demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview)
