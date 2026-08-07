CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME loadingframe-demo
    TITLE "Frame With Loading Spinner"
    DESCRIPTION "Frame that shows a loading spinner while its content is unavailable."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
