CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME editablelabel-demo
    TITLE "Editable Label"
    DESCRIPTION "Typed value widgets (text, int, double, date, combo, ...) with an inline edit/apply cycle."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
