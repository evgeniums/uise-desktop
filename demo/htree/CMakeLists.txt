CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(horizontaltree-demo LANGUAGES CXX)

SET (HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/dirlist.hpp
)

SET (SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)

ADD_EXECUTABLE(${PROJECT_NAME} ${HEADERS} ${SOURCES})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${CMAKE_PROJECT_NAME})

