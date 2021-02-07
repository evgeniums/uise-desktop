CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(flyweightlistview-demo LANGUAGES CXX)

SET (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/helloworlditem.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/fwlvtestwidget.hpp
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview/fwlvtestwidget.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

ADD_EXECUTABLE(${PROJECT_NAME} ${HEADERS} ${SOURCES})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CMAKE_PROJECT_NAME})

TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../test/flyweightlistview)
