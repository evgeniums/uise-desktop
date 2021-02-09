CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(uise-desktop VERSION 0.0.1 LANGUAGES CXX)

SET(CMAKE_CXX_STANDARD 17)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

FIND_PACKAGE(Boost 1.65 REQUIRED)

IF ("$ENV{QT_HOME}" STREQUAL "")
ELSE()
    MESSAGE (STATUS "Using Qt from $ENV{QT_HOME}")
    SET (QT_HOME $ENV{QT_HOME})
    SET (Qt6_DIR ${QT_HOME}/lib/cmake/Qt6)
    SET (Qt6CoreTools_DIR ${QT_HOME}/lib/cmake/Qt6CoreTools)
    SET (Qt6GuiTools_DIR ${QT_HOME}/lib/cmake/Qt6GuiTools)
    SET (Qt6WidgetsTools_DIR ${QT_HOME}/lib/cmake/Qt6WidgetsTools)
ENDIF()

FIND_PACKAGE(Qt6 COMPONENTS Widgets REQUIRED)

OPTION(UISE_DESKTOP_STATIC "Build static uise-desktop library" OFF)
OPTION(UISE_DESKTOP_DEMO "Build demo appliations using uise-desktop UI" ON)
OPTION(UISE_DESKTOP_TEST "Build unit tests for uise-desktop UI" ON)
OPTION(UISE_TEST_JUNIT "Write tests result to XML file for JUnit" OFF)

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

    include/uise/desktop/alignedstretchingwidget.hpp
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
    src/alignedstretchingwidget.cpp
)

IF (NOT UISE_DESKTOP_STATIC)
    IF (NOT "$ENV{UISE_DESKTOP_STATIC}" STREQUAL "")
        SET (UISE_DESKTOP_STATIC $ENV{UISE_DESKTOP_STATIC})
    ENDIF()
ENDIF()

IF (UISE_DESKTOP_STATIC)
    MESSAGE(STATUS "Building static uise-desktop library")
    ADD_LIBRARY(${PROJECT_NAME} STATIC ${HEADERS} ${SOURCES})
    TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC "-DUISE_DESKTOP_EXPORT=")
ELSE (UISE_DESKTOP_STATIC)
    MESSAGE(STATUS "Building shared uise-desktop library")
    ADD_LIBRARY(${PROJECT_NAME} SHARED ${HEADERS} ${SOURCES})
    TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -DUISE_DESKTOP_BUILD)
ENDIF(UISE_DESKTOP_STATIC)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Qt6::Widgets)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIR})

IF (UISE_DESKTOP_DEMO)
    ADD_SUBDIRECTORY(demo)
ENDIF (UISE_DESKTOP_DEMO)

IF (UISE_DESKTOP_TEST)
    ENABLE_TESTING(true)

    TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC "-DUISE_TEST_JUNIT_PATH=\"${CMAKE_BINARY_DIR}/junit/\"")
    IF (UISE_TEST_JUNIT)
        MESSAGE(STATUS "Tests will write JUnit XML files")
        FILE(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/junit")
        TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PUBLIC -DUISE_TEST_JUNIT)
    ENDIF(UISE_TEST_JUNIT)

    ADD_SUBDIRECTORY(test)
ENDIF (UISE_DESKTOP_TEST)

SET (BUILD_CI_FILES
    build/windows-ci.bat
    build/mingw-config.bat
    build/msvc-config.bat
    build/unix-ci.sh
)

ADD_CUSTOM_TARGET(build SOURCES ${BUILD_CI_FILES})
