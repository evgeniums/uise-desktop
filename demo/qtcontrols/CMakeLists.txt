CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME qtcontrols-demo
    TITLE "Standard Qt Controls"
    DESCRIPTION "Native Qt controls not redefined by uise (combobox, spinbox, date/time editors, tabs, item views, menus...), styled purely via reset.qss/global.qss."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    PRIORITY 0
)
