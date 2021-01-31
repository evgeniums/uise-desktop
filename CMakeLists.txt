CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(uise-desktop VERSION 0.0.1 LANGUAGES CXX)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

FIND_PACKAGE(Boost 1.65 REQUIRED)
FIND_PACKAGE(Qt6 COMPONENTS Widgets REQUIRED)

OPTION(UISE_DESKTOP_STATIC "Build static uise-desktop library" OFF)
OPTION(UISE_DESKTOP_DEMO "Build demo appliations using uise-desktop UI" ON)
OPTION(STANDALONE_BOOST_MULTI_INDEX "Use standalone version of boost multin_index_container in 3rdparty" ON)

SET(UISE_DESKTOP_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

INCLUDE_DIRECTORIES(${INCLUDE_DIRECTORIES} ${CMAKE_CURRENT_SOURCE_DIR}/include)

ADD_SUBDIRECTORY(3rdparty)

SET (HEADERS
    include/uise/desktop/uisedesktop.hpp
    include/uise/desktop/version.hpp

    include/uise/desktop/utils/layout.hpp
    include/uise/desktop/utils/destroywidget.hpp
    include/uise/desktop/utils/pointerholder.hpp
    include/uise/desktop/utils/singleshottimer.hpp
    include/uise/desktop/utils/orientationinvariant.hpp
    include/uise/desktop/utils/directchildwidget.hpp

    include/uise/desktop/linkedlistview.hpp
    include/uise/desktop/linkedlistviewitem.hpp

    include/uise/desktop/flyweightlistitem.hpp
    include/uise/desktop/flyweightlistframe.hpp
    include/uise/desktop/flyweightlistview.hpp
    include/uise/desktop/flyweightlistview.ipp
)

SET (HEADERS ${HEADERS}
    include/uise/desktop/detail/flyweightlistview_p.hpp
    include/uise/desktop/detail/flyweightlistview_p.ipp
)

SET (SOURCES
    src/linkedlistviewitem.cpp
    src/linkedlistview.cpp
    src/flyweightlistframe.cpp
    src/singleshottimer.cpp
    src/directchildwidget.cpp
)

IF (UISE_DESKTOP_STATIC)
    MESSAGE(STATUS "Building static uise-desktop library")
    ADD_LIBRARY(${PROJECT_NAME} STATIC ${HEADERS} ${SOURCES})
    ADD_DEFINITIONS("-DUISE_DESKTOP_EXPORT=")
ELSE (UISE_DESKTOP_STATIC)
    MESSAGE(STATUS "Building shared uise-desktop library")
    ADD_LIBRARY(${PROJECT_NAME} SHARED ${HEADERS} ${SOURCES})
    ADD_DEFINITIONS("-DUISE_DESKTOP_BUILD")
ENDIF(UISE_DESKTOP_STATIC)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Qt6::Widgets)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIR})

IF (UISE_DESKTOP_DEMO)
    ADD_SUBDIRECTORY(demo)
ENDIF (UISE_DESKTOP_DEMO)
