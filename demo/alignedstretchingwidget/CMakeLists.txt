CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME alignedstretchingwidget-demo
    TITLE "Aligned Stretching Widget"
    DESCRIPTION "Stretches and aligns a child widget within a container while preserving its aspect ratio."
    HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/alignedstretchingwidgetdemo.hpp
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
