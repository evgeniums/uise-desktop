CMAKE_MINIMUM_REQUIRED (VERSION 3.16)

uise_demo(
    NAME chatimageviewer-flyweight-demo
    TITLE "Chat Image Viewer (Flyweight)"
    DESCRIPTION "Windowed chat image viewer: prefetching, a blurry-to-sharp version ladder, the loading overlay, and both album/continuous thumbnail-strip scopes."
    SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/demoflyweightbackend.hpp
)
