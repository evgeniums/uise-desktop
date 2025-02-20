CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(uisedesktop VERSION 0.0.1 LANGUAGES CXX)

# SET(CMAKE_CXX_STANDARD 17)

SET(CMAKE_CXX_STANDARD_REQUIRED ON)

SET(CMAKE_AUTOMOC ON)
SET(CMAKE_AUTORCC ON)
SET(CMAKE_AUTOUIC ON)

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

GET_DIRECTORY_PROPERTY(HasParent PARENT_DIRECTORY)
IF(HasParent)
    OPTION(UISE_DESKTOP_DEMO "Build demo appliations using uise-desktop UI" OFF)
    OPTION(UISE_DESKTOP_TEST "Build unit tests for uise-desktop UI" OFF)
ELSE()
    OPTION(UISE_DESKTOP_DEMO "Build demo appliations using uise-desktop UI" ON)
    OPTION(UISE_DESKTOP_TEST "Build unit tests for uise-desktop UI" ON)
ENDIF()
OPTION(UISE_TEST_JUNIT "Write tests result to XML file for JUnit" OFF)

SET(UISE_DESKTOP_TOP_DIR ${CMAKE_CURRENT_SOURCE_DIR})

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
    include/uise/desktop/utils/substitutecolors.hpp
    include/uise/desktop/utils/wheeleventhandler.hpp

    include/uise/desktop/linkedlistview.hpp
    include/uise/desktop/linkedlistviewitem.hpp

    include/uise/desktop/flyweightlistitem.hpp
    include/uise/desktop/flyweightlistview.hpp
    include/uise/desktop/flyweightlistview.ipp

    include/uise/desktop/alignedstretchingwidget.hpp
    include/uise/desktop/editablelabel.hpp
    include/uise/desktop/style.hpp

    include/uise/desktop/spinner.hpp
    include/uise/desktop/spinnersection.hpp

    include/uise/desktop/busywaiting.hpp
)

SET (HEADERS ${HEADERS}
    include/uise/desktop/detail/flyweightlistview_p.hpp
    include/uise/desktop/detail/flyweightlistview_p.ipp

    include/uise/desktop/detail/spinnersection_p.hpp
)

SET (SOURCES
    src/linkedlistviewitem.cpp
    src/linkedlistview.cpp
    src/singleshottimer.cpp
    src/directchildwidget.cpp
    src/alignedstretchingwidget.cpp
    src/editablelabel.cpp
    src/style.cpp
    src/spinner.cpp
    src/spinnersection.cpp
    src/wheeleventhandler.cpp
    src/busywaiting.cpp
)

QT6_ADD_RESOURCES(SOURCES resources/uise.qrc)

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

OPTION(BOOST_STATIC "Use static Boost libraries" OFF)
IF (BOOST_STATIC)
    SET (Boost_USE_STATIC_LIBS ON CACHE BOOL "Boost static libs")
ELSE (BOOST_STATIC)
    SET (Boost_USE_STATIC_LIBS OFF CACHE BOOL "Boost static libs")
    SET(Boost_USE_STATIC_RUNTIME OFF)
    IF(WIN32)
        TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -DBOOST_ALL_NO_LIB)
    ENDIF(WIN32)
    TARGET_COMPILE_DEFINITIONS(${PROJECT_NAME} PRIVATE -DBOOST_ALL_DYN_LINK)
ENDIF (BOOST_STATIC)

FIND_PACKAGE(Boost CONFIG 1.65 REQUIRED)

# MESSAGE (STATUS "Boost_INCLUDE_DIRS: ${Boost_INCLUDE_DIRS}")

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Qt6::Widgets)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

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
