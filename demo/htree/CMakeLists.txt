CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME horizontaltree-demo
    TITLE "HTree Filesystem"
    DESCRIPTION "Horizontal tree browser navigating the local filesystem."
    HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/dirlist.hpp
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
