CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(uisedesktop VERSION 0.1.1 LANGUAGES CXX)

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

FIND_PACKAGE(Qt6 COMPONENTS Core Widgets Svg REQUIRED)

SET(UISE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

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

SET (HEADERS
    include/uise/desktop/uisedesktop.hpp
    include/uise/desktop/version.hpp

    include/uise/desktop/widget.hpp
    include/uise/desktop/widgetfactory.hpp

    include/uise/desktop/utils/assert.hpp
    include/uise/desktop/utils/layout.hpp
    include/uise/desktop/utils/destroywidget.hpp
    include/uise/desktop/utils/pointerholder.hpp
    include/uise/desktop/utils/singleshottimer.hpp
    include/uise/desktop/utils/orientationinvariant.hpp
    include/uise/desktop/utils/directchildwidget.hpp
    include/uise/desktop/utils/substitutecolors.hpp
    include/uise/desktop/utils/wheeleventhandler.hpp
    include/uise/desktop/utils/makenullwidget.hpp
    include/uise/desktop/utils/withpathandsize.hpp
    include/uise/desktop/utils/enums.hpp

    include/uise/desktop/linkedlistview.hpp
    include/uise/desktop/linkedlistviewitem.hpp

    include/uise/desktop/flyweightlistitem.hpp
    include/uise/desktop/flyweightlistview.hpp    

    include/uise/desktop/alignedstretchingwidget.hpp

    include/uise/desktop/valuewidget.hpp
    include/uise/desktop/editablelabel.hpp

    include/uise/desktop/style.hpp

    include/uise/desktop/elidedlabel.hpp

    include/uise/desktop/spinner.hpp
    include/uise/desktop/spinnersection.hpp

    include/uise/desktop/busywaiting.hpp

    include/uise/desktop/modalpopup.hpp
    include/uise/desktop/drawer.hpp

    include/uise/desktop/abstractdialog.hpp
    include/uise/desktop/dialog.hpp
    include/uise/desktop/modaldialog.hpp
    include/uise/desktop/statusdialog.hpp

    include/uise/desktop/framewithmodalstatus.hpp

    include/uise/desktop/framewithrefresh.hpp

    include/uise/desktop/scrollarea.hpp
    include/uise/desktop/navigationbar.hpp
    include/uise/desktop/stackwithnavigationbar.hpp

    include/uise/desktop/htreepath.hpp
    include/uise/desktop/htreenode.hpp
    include/uise/desktop/htreenodelocator.hpp
    include/uise/desktop/htreenodebuilder.hpp
    include/uise/desktop/htreenodefactory.hpp
    include/uise/desktop/htreebranch.hpp    
    include/uise/desktop/htreesidebar.hpp
    include/uise/desktop/htreetab.hpp
    include/uise/desktop/htreesplitter.hpp
    include/uise/desktop/htree.hpp

    include/uise/desktop/htreelist.hpp
    include/uise/desktop/htreelistitem.hpp
    include/uise/desktop/htreelistitemtemplate.hpp
    include/uise/desktop/htreeflyweightlistitem.hpp
    include/uise/desktop/htreestandardlistitem.hpp
    include/uise/desktop/htreelistwidget.hpp
    include/uise/desktop/htreelistflyweightview.hpp

    include/uise/desktop/svgicon.hpp
    include/uise/desktop/svgiconcontext.hpp
    include/uise/desktop/svgiconlocator.hpp    

    include/uise/desktop/stylecontext.hpp

    include/uise/desktop/pushbutton.hpp

    include/uise/desktop/editablepanel.hpp
    include/uise/desktop/editablepanelgrid.hpp
    include/uise/desktop/editablepanels.hpp

    include/uise/desktop/defaultwidgetfactory.hpp

    include/uise/desktop/passwordinput.hpp
    include/uise/desktop/passwordpanel.hpp
    include/uise/desktop/passworddialog.hpp

    include/uise/desktop/wizarddialog.hpp

    include/uise/desktop/newpasswordwizard.hpp

    include/uise/desktop/pixmapproducer.hpp
    include/uise/desktop/roundedimage.hpp
    include/uise/desktop/avatar.hpp

    include/uise/desktop/loadingframe.hpp
    include/uise/desktop/elidedcontainer.hpp

    include/uise/desktop/abstractimageeditor.hpp
    include/uise/desktop/simpleimageeditor.hpp
    include/uise/desktop/imagecropper.hpp
    include/uise/desktop/freehanddrawview.hpp
)

SET (HEADERS ${HEADERS}
    include/uise/desktop/detail/flyweightlistview_p.hpp
    include/uise/desktop/detail/flyweightlistview_p.ipp

    include/uise/desktop/detail/spinnersection_p.hpp

    include/uise/desktop/detail/htreesplitter_p.hpp
)

SET (HEADERS ${HEADERS}
    include/uise/desktop/ipp/dialog.ipp
    include/uise/desktop/ipp/flyweightlistview.ipp
    include/uise/desktop/ipp/htreeflyweightlistitem.ipp
    include/uise/desktop/ipp/htreelistitemtemplate.ipp
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
    src/modalpopup.cpp
    src/drawer.cpp
    src/scrollarea.cpp
    src/framewithrefresh.cpp
    src/navigationbar.cpp
    src/stackwithnavigationbar.cpp
    src/elidedlabel.cpp

    src/htree.cpp
    src/htreenode.cpp
    src/htreenodefactory.cpp
    src/htreenodelocator.cpp
    src/htreebranch.cpp
    src/htreelistitem.cpp
    src/htreeflyweightlistitem.cpp
    src/htreestandardlistitem.cpp
    src/htreelist.cpp
    src/htreetab.cpp
    src/htreesplitter.cpp
    src/htreesidebar.cpp

    src/svgicon.cpp
    src/svgiconcontext.cpp
    src/svgiconlocator.cpp

    src/pushbutton.cpp

    src/editablepanel.cpp
    src/editablepanelgrid.cpp
    src/editablepanels.cpp

    src/abstractdialog.cpp
    src/statusdialog.cpp

    src/framewithmodalstatus.cpp

    src/widget.cpp
    src/widgetfactory.cpp

    src/defaultwidgetfactory.cpp

    src/passwordinput.cpp
    src/passwordpanel.cpp
    src/passworddialog.cpp

    src/wizarddialog.cpp

    src/newpasswordwizard.cpp

    src/pixmapproducer.cpp
    src/roundedimage.cpp
    src/avatar.cpp

    src/loadingframe.cpp
    src/elidedcontainer.cpp

    src/abstractimageeditor.cpp
    src/simpleimageeditor.cpp
    src/imagecropper.cpp
    src/freehanddrawview.cpp
)

QT6_ADD_RESOURCES(SOURCES resources/uise.qrc)
QT6_ADD_RESOURCES(SOURCES thirdparty/file-icon-vectors.qrc)
QT6_ADD_RESOURCES(SOURCES thirdparty/tabler-icons.qrc)
QT6_ADD_RESOURCES(SOURCES thirdparty/reshot.qrc)

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

IF (NOT Boost_DIR)
    IF ($ENV{Boost_DIR})
        SET(Boost_DIR $ENV{Boost_DIR})
    ENDIF()
ENDIF()

FIND_PACKAGE(Boost CONFIG 1.65 REQUIRED)

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Qt6::Core Qt6::Widgets Qt6::Svg)
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

IF (BUILD_STATIC)
    INSTALL(TARGETS ${PROJECT_NAME}
            ARCHIVE DESTINATION lib
    )
ELSE (BUILD_STATIC)
    IF (INSTALL_DEV)
        INSTALL(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
            ARCHIVE DESTINATION lib
        )
    ELSE(INSTALL_DEV)
        INSTALL(TARGETS ${PROJECT_NAME}
            RUNTIME DESTINATION bin
            LIBRARY DESTINATION lib
        )
    ENDIF(INSTALL_DEV)
ENDIF(BUILD_STATIC)

IF (INSTALL_DEV)
    INSTALL(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/uise DESTINATION include)
ENDIF(INSTALL_DEV)

INCLUDE(thirdparty/thirdparty.cmake)
