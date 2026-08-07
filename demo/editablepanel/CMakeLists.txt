CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME editablepanel-demo
    TITLE "Editable Panel"
    DESCRIPTION "Form panel built from EditablePanel/EditablePanelGrid rows."
    HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/demopanel.hpp
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
)
