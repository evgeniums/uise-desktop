CMAKE_MINIMUM_REQUIRED (VERSION 3.16)
PROJECT(uisedesktop VERSION 1.0.1 LANGUAGES CXX C ASM)

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
    SET (Qt6LinguistTools_DIR ${QT_HOME}/lib/cmake/Qt6LinguistTools)
ENDIF()

FIND_PACKAGE(Qt6 COMPONENTS Core Widgets Svg LinguistTools REQUIRED)

ADD_SUBDIRECTORY(thirdparty)

SET(UISE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})

OPTION(UISE_DESKTOP_STATIC "Build static uise-desktop library" OFF)
OPTION(UISE_DESKTOP_MULTIMEDIA "Enable multimedia classesy" ON)

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

    include/uise/desktop/frame.hpp
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
    include/uise/desktop/utils/graphicsviewzoom.hpp
    include/uise/desktop/utils/makenullwidget.hpp
    include/uise/desktop/utils/withpathandsize.hpp
    include/uise/desktop/utils/enums.hpp
    include/uise/desktop/utils/datetime.hpp
    include/uise/desktop/utils/filesizeformat.hpp
    include/uise/desktop/utils/filetypeicon.hpp
    include/uise/desktop/utils/pixmapscale.hpp
    include/uise/desktop/utils/albumlayout.hpp
    include/uise/desktop/utils/mimedatautils.hpp
    include/uise/desktop/utils/dragsource.hpp

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
    include/uise/desktop/datetimepickersimple.hpp
    include/uise/desktop/datetimepicker.hpp
    include/uise/desktop/datetimeinput.hpp
    include/uise/desktop/calendar.hpp
    include/uise/desktop/calendarinput.hpp

    include/uise/desktop/busywaiting.hpp
    include/uise/desktop/abstractloadingwidget.hpp
    include/uise/desktop/busywaitingframe.hpp
    include/uise/desktop/circlebusyframe.hpp
    include/uise/desktop/skeleton.hpp
    include/uise/desktop/typingindicator.hpp

    include/uise/desktop/modalpopup.hpp
    include/uise/desktop/drawer.hpp

    include/uise/desktop/abstractdialog.hpp
    include/uise/desktop/dialog.hpp
    include/uise/desktop/modaldialog.hpp
    include/uise/desktop/floatingdialog.hpp
    include/uise/desktop/statusdialog.hpp
    include/uise/desktop/calendardialog.hpp

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
    include/uise/desktop/htreetabbar.hpp
    include/uise/desktop/htreesplitter.hpp
    include/uise/desktop/htree.hpp

    include/uise/desktop/htreelist.hpp
    include/uise/desktop/htreelistitem.hpp
    include/uise/desktop/htreelistitemexport.hpp
    include/uise/desktop/htreelistitemtemplate.hpp
    include/uise/desktop/htreeflyweightlistitem.hpp
    include/uise/desktop/htreestandardlistitem.hpp
    include/uise/desktop/htreecontrollerlistitem.hpp
    include/uise/desktop/htreelistwidget.hpp
    include/uise/desktop/htreelistflyweightview.hpp

    include/uise/desktop/svgicon.hpp
    include/uise/desktop/svgiconcontext.hpp
    include/uise/desktop/svgiconlocator.hpp    

    include/uise/desktop/stylecontext.hpp

    include/uise/desktop/pushbutton.hpp

    include/uise/desktop/audiodevicesetting.hpp
    include/uise/desktop/editablepanel.hpp
    include/uise/desktop/editablepanelgrid.hpp
    include/uise/desktop/editablepanels.hpp

    include/uise/desktop/defaultwidgetfactory.hpp

    include/uise/desktop/passwordinput.hpp
    include/uise/desktop/passwordpanel.hpp
    include/uise/desktop/passworddialog.hpp

    include/uise/desktop/wizarddialog.hpp

    include/uise/desktop/newpasswordwizard.hpp

    include/uise/desktop/imageanimator.hpp
    include/uise/desktop/pixmapproducer.hpp
    include/uise/desktop/roundedimage.hpp
    include/uise/desktop/imagelabel.hpp
    include/uise/desktop/avatar.hpp
    include/uise/desktop/avatarbutton.hpp

    include/uise/desktop/loadingframe.hpp
    include/uise/desktop/elidedcontainer.hpp

    include/uise/desktop/abstractimageeditor.hpp
    include/uise/desktop/simpleimageeditor.hpp
    include/uise/desktop/imagecropper.hpp
    include/uise/desktop/freehanddrawview.hpp
    include/uise/desktop/imageeditdialog.hpp

    include/uise/desktop/jumpedge.hpp

    include/uise/desktop/newpasswordpanel.hpp
    include/uise/desktop/newpassworddialog.hpp
    include/uise/desktop/toast.hpp

    include/uise/desktop/qrbarcode.hpp
    include/uise/desktop/qrcodefromimage.hpp

    include/uise/desktop/icontextbutton.hpp
    include/uise/desktop/ripple.hpp
    include/uise/desktop/abstractcheckbox.hpp
    include/uise/desktop/checkbox.hpp
    include/uise/desktop/radiobox.hpp
    include/uise/desktop/abstractloadcontrol.hpp
    include/uise/desktop/loadcontrol.hpp
    include/uise/desktop/loadcontrolmenu.hpp
    include/uise/desktop/dropdownframe.hpp
    include/uise/desktop/dropdownmenu.hpp
    include/uise/desktop/fastswitchbutton.hpp
    include/uise/desktop/fileuploaditem.hpp
    include/uise/desktop/fileuploadlistitem.hpp
    include/uise/desktop/abstractfileuploadwidget.hpp
    include/uise/desktop/fileuploadwidget.hpp
    include/uise/desktop/fileuploaddialog.hpp
    include/uise/desktop/filedropoverlay.hpp

    include/uise/desktop/abstractimageviewer.hpp
    include/uise/desktop/imageviewer.hpp
    include/uise/desktop/directoryimagesviewer.hpp
    include/uise/desktop/imagepreviewstrip.hpp
    include/uise/desktop/chatimageviewercontrols.hpp
    include/uise/desktop/chatimageviewer.hpp
    include/uise/desktop/chatimageviewerwindow.hpp

    include/uise/desktop/circlebusy.hpp
    include/uise/desktop/lineedit.hpp
    include/uise/desktop/searchlineedit.hpp

    include/uise/desktop/buttonslist.hpp
    include/uise/desktop/validatedinput.hpp

    include/uise/desktop/label.hpp

    include/uise/desktop/abstractchatmessage.hpp
    include/uise/desktop/abstractchatmessagecall.hpp
    include/uise/desktop/chatmessagetext.hpp
    include/uise/desktop/chatmessagecall.hpp
    include/uise/desktop/chatmessage.hpp
    include/uise/desktop/chatdatesubtitle.hpp
    include/uise/desktop/chatfloatingavatar.hpp
    include/uise/desktop/chatmessagesview.hpp

    include/uise/desktop/chatfileitem.hpp
    include/uise/desktop/chatmessagefileitem.hpp
    include/uise/desktop/abstractchatmessagefiles.hpp
    include/uise/desktop/chatmessagefiles.hpp
    include/uise/desktop/chatmessageimageitem.hpp
    include/uise/desktop/abstractchatmessageimages.hpp
    include/uise/desktop/chatmessageimages.hpp

    include/uise/desktop/replypreviewdata.hpp
    include/uise/desktop/abstractreplypreview.hpp
    include/uise/desktop/replypreview.hpp
    include/uise/desktop/chatmessagereply.hpp
    include/uise/desktop/abstractreplybar.hpp
    include/uise/desktop/replybar.hpp
    include/uise/desktop/abstractreplydialog.hpp
    include/uise/desktop/replydialog.hpp

    include/uise/desktop/chatmessagecomment.hpp
    include/uise/desktop/chatmessageforwardheader.hpp
    include/uise/desktop/abstractforwardbar.hpp
    include/uise/desktop/forwardbar.hpp
    include/uise/desktop/abstractforwarddialog.hpp
    include/uise/desktop/forwarddialog.hpp

    include/uise/desktop/abstracteditbar.hpp
    include/uise/desktop/editbar.hpp

    include/uise/desktop/abstractmessageeditor.hpp
    include/uise/desktop/messageeditor.hpp
    include/uise/desktop/messageselectionpanel.hpp

    include/uise/desktop/verticalscrollbar.hpp
)

SET (HEADERS ${HEADERS}
    include/uise/desktop/detail/flyweightlistview_p.hpp
    include/uise/desktop/detail/flyweightlistview_p.ipp

    include/uise/desktop/detail/spinnersection_p.hpp
    include/uise/desktop/detail/datetimepicker_p.hpp
    include/uise/desktop/detail/calendar_p.hpp

    include/uise/desktop/detail/htreesplitter_p.hpp
)

SET (HEADERS ${HEADERS}
    include/uise/desktop/ipp/dialog.ipp
    include/uise/desktop/ipp/flyweightlistview.ipp
    include/uise/desktop/ipp/htreeflyweightlistitem.ipp
    include/uise/desktop/ipp/htreelistitemtemplate.ipp
    include/uise/desktop/ipp/chatmessagesview.ipp
)

SET (SOURCES
    src/linkedlistviewitem.cpp
    src/linkedlistview.cpp
    src/singleshottimer.cpp
    src/directchildwidget.cpp
    src/mimedatautils.cpp
    src/dragsource.cpp
    src/alignedstretchingwidget.cpp
    src/editablelabel.cpp
    src/style.cpp
    src/spinner.cpp
    src/spinnersection.cpp
    src/datetimepickersimple.cpp
    src/datetimepicker.cpp
    src/datetimeinput.cpp
    src/calendar.cpp
    src/calendarinput.cpp
    src/wheeleventhandler.cpp
    src/graphicsviewzoom.cpp
    src/busywaiting.cpp
    src/abstractloadingwidget.cpp
    src/busywaitingframe.cpp
    src/circlebusyframe.cpp
    src/skeleton.cpp
    src/typingindicator.cpp
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
    src/htreetabbar.cpp
    src/htreesplitter.cpp
    src/htreesidebar.cpp

    src/svgicon.cpp
    src/svgiconcontext.cpp
    src/svgiconlocator.cpp

    src/pushbutton.cpp

    src/audiodevicesetting.cpp
    src/editablepanel.cpp
    src/editablepanelgrid.cpp
    src/editablepanels.cpp

    src/abstractdialog.cpp
    src/floatingdialog.cpp
    src/statusdialog.cpp
    src/calendardialog.cpp

    src/framewithmodalstatus.cpp

    src/widget.cpp
    src/widgetfactory.cpp

    src/defaultwidgetfactory.cpp

    src/passwordinput.cpp
    src/passwordpanel.cpp
    src/passworddialog.cpp

    src/wizarddialog.cpp

    src/newpasswordwizard.cpp

    src/imageanimator.cpp
    src/pixmapproducer.cpp
    src/roundedimage.cpp
    src/imagelabel.cpp
    src/avatar.cpp
    src/avatarbutton.cpp

    src/loadingframe.cpp
    src/elidedcontainer.cpp

    src/abstractimageeditor.cpp
    src/simpleimageeditor.cpp
    src/imagecropper.cpp
    src/freehanddrawview.cpp
    src/imageeditdialog.cpp

    src/jumpedge.cpp

    src/newpasswordpanel.cpp
    src/newpassworddialog.cpp
    src/toast.cpp

    src/qrcodefromimage.cpp
    src/icontextbutton.cpp
    src/ripple.cpp
    src/abstractcheckbox.cpp
    src/checkbox.cpp
    src/radiobox.cpp
    src/loadcontrol.cpp
    src/loadcontrolmenu.cpp
    src/dropdownframe.cpp
    src/dropdownmenu.cpp
    src/fastswitchbutton.cpp
    src/filetypeicon.cpp
    src/fileuploaditem.cpp
    src/fileuploadlistitem.cpp
    src/fileuploadwidget.cpp
    src/fileuploaddialog.cpp
    src/filedropoverlay.cpp

    src/abstractimageviewer.cpp
    src/imageviewer.cpp
    src/directoryimagesviewer.cpp
    src/imagepreviewstrip.cpp
    src/chatimageviewercontrols.cpp
    src/chatimageviewer.cpp
    src/chatimageviewerwindow.cpp

    src/circlebusy.cpp
    src/lineedit.cpp
    src/searchlineedit.cpp

    src/buttonslist.cpp
    src/validatedinput.cpp

    src/label.cpp
    src/abstractchatmessagecall.cpp
    src/chatmessagetext.cpp
    src/chatmessagecall.cpp
    src/chatmessage.cpp
    src/chatdatesubtitle.cpp
    src/chatfloatingavatar.cpp

    src/albumlayout.cpp
    src/chatfileitem.cpp
    src/chatmessagefileitem.cpp
    src/chatmessagefiles.cpp
    src/chatmessageimageitem.cpp
    src/chatmessageimages.cpp

    src/replypreviewdata.cpp
    src/replypreview.cpp
    src/chatmessagereply.cpp
    src/replybar.cpp
    src/abstractreplydialog.cpp
    src/replydialog.cpp

    src/chatmessagecomment.cpp
    src/chatmessageforwardheader.cpp
    src/forwardbar.cpp
    src/abstractforwarddialog.cpp
    src/forwarddialog.cpp

    src/editbar.cpp

    src/messageeditor.cpp
    src/messageselectionpanel.cpp
    src/verticalscrollbar.cpp
)

IF (UISE_DESKTOP_MULTIMEDIA)

    LIST (APPEND HEADERS
        include/uise/desktop/qrcodefromvideo.hpp
        include/uise/desktop/qrcodescanner.hpp
    )

    LIST (APPEND SOURCES
        src/qrcodefromvideo.cpp
        src/qrcodescanner.cpp
    )

ENDIF()

LIST (APPEND HEADERS
    include/uise/desktop/i18n.hpp
)

LIST (APPEND SOURCES
    src/i18n.cpp
)

QT6_ADD_RESOURCES(SOURCES resources/uise.qrc)
QT6_ADD_RESOURCES(SOURCES thirdparty/file-icon-vectors.qrc)
QT6_ADD_RESOURCES(SOURCES thirdparty/tabler-icons.qrc)
QT6_ADD_RESOURCES(SOURCES thirdparty/reshot.qrc)

# ---------------------------------------------------------------------------
# i18n: compile translations/uise_<lang>.ts -> <build>/translations/uise_<lang>.qm
# and embed them at :/uise/desktop/translations/.
#
# lrelease runs at BUILD time, not configure time: the generated qrc lists each
# .qm by absolute build-dir path, and qt6_add_resources's configure-time qrc
# parser folds every <file> path into the rcc custom command's DEPENDS, so this
# wires lrelease -> rcc -> link automatically. Editing a .ts and rebuilding
# picks it up with no cmake re-run.
#
# No language list here (unlike whitemdesktop/translations/languages.txt) --
# uise-desktop is a standalone public repo and simply builds whatever
# translations/uise_*.ts files exist, so it stays decoupled from any label's
# language policy.
# ---------------------------------------------------------------------------
SET(_uise_ts_dir "${CMAKE_CURRENT_SOURCE_DIR}/translations")
FILE(GLOB _uise_ts_files CONFIGURE_DEPENDS "${_uise_ts_dir}/uise_*.ts")
LIST(SORT _uise_ts_files)

SET(_uise_qm_dir "${CMAKE_CURRENT_BINARY_DIR}/translations")
FILE(MAKE_DIRECTORY "${_uise_qm_dir}")

# Drafts must never ship: -nounfinished drops every message not marked finished
# in Linguist, so an empty/partial translation falls through to the English
# source string instead of rendering as an empty label.
SET(_uise_lrelease_opts -silent -nounfinished)
IF(UISE_DESKTOP_I18N_INCLUDE_UNFINISHED)
    SET(_uise_lrelease_opts -silent)
ENDIF()

IF(_uise_ts_files)
    SET(_uise_qm_files "")
    SET(_uise_langs "")
    SET(_uise_tr_body "<RCC>\n  <qresource prefix=\"/uise/desktop/translations\">\n")
    FOREACH(_uise_ts IN LISTS _uise_ts_files)
        GET_FILENAME_COMPONENT(_uise_ts_base "${_uise_ts}" NAME_WE)     # uise_ru
        STRING(REGEX REPLACE "^uise_" "" _uise_lang "${_uise_ts_base}") # ru
        SET(_uise_qm "${_uise_qm_dir}/${_uise_ts_base}.qm")
        ADD_CUSTOM_COMMAND(
            OUTPUT  "${_uise_qm}"
            COMMAND Qt6::lrelease ${_uise_lrelease_opts} "${_uise_ts}" -qm "${_uise_qm}"
            DEPENDS "${_uise_ts}" Qt6::lrelease
            COMMENT "lrelease ${_uise_ts_base}.qm"
            VERBATIM)
        LIST(APPEND _uise_qm_files "${_uise_qm}")
        LIST(APPEND _uise_langs "${_uise_lang}")
        STRING(APPEND _uise_tr_body "    <file alias=\"${_uise_ts_base}.qm\">${_uise_qm}</file>\n")
    ENDFOREACH()

    # Always-present manifest keeps the qrc non-empty (rcc warns on an empty
    # <qresource>) and lets availableTranslations() report which catalogs this
    # build actually carries.
    STRING(REPLACE ";" "\",\"" _uise_langs_json "${_uise_langs}")
    FILE(WRITE "${_uise_qm_dir}/manifest.json" "{\"languages\":[\"${_uise_langs_json}\"]}\n")
    STRING(APPEND _uise_tr_body "    <file alias=\"manifest.json\">${_uise_qm_dir}/manifest.json</file>\n")
    STRING(APPEND _uise_tr_body "  </qresource>\n</RCC>\n")

    # Write via a .in + CONFIGURE_FILE(COPYONLY): configure_file only rewrites
    # when the content differs, so a no-op cmake re-run does not touch the qrc
    # mtime and does not trigger a spurious rcc + recompile.
    SET(_uise_tr_qrc "${CMAKE_CURRENT_BINARY_DIR}/uise_translations.qrc")
    FILE(WRITE "${_uise_tr_qrc}.in" "${_uise_tr_body}")
    CONFIGURE_FILE("${_uise_tr_qrc}.in" "${_uise_tr_qrc}" COPYONLY)

    QT6_ADD_RESOURCES(SOURCES "${_uise_tr_qrc}")

    # Visible in IDEs, and a manual hook (`cmake --build . --target uisedesktop-translations`).
    ADD_CUSTOM_TARGET(uisedesktop-translations
        DEPENDS ${_uise_qm_files}
        SOURCES ${_uise_ts_files})
ELSE()
    MESSAGE(STATUS "uise-desktop: no translations/uise_*.ts found -- shipping English only")
ENDIF()

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

IF(TARGET uisedesktop-translations)
    ADD_DEPENDENCIES(${PROJECT_NAME} uisedesktop-translations)
ENDIF()

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

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Qt6::Core Qt6::Widgets Qt6::Svg ZXing)
TARGET_INCLUDE_DIRECTORIES(${PROJECT_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include ${Boost_INCLUDE_DIRS})

IF (UISE_DESKTOP_MULTIMEDIA)
    FIND_PACKAGE(Qt6 COMPONENTS Multimedia MultimediaWidgets REQUIRED)
    TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC Qt6::Multimedia Qt6::MultimediaWidgets)
ENDIF()

TARGET_LINK_LIBRARIES(${PROJECT_NAME} PUBLIC QtColorWidgets)

IF (NOT MSVC)
    TARGET_COMPILE_OPTIONS(${PROJECT_NAME} PRIVATE
        -Wextra
        -Wall
        -Wnon-virtual-dtor
        -Wreorder-ctor
        -Wno-unused-lambda-capture
        -Wunused-variable
    )
ENDIF (NOT MSVC)

IF (UISE_DESKTOP_DEMO)
    SET(UISE_DESKTOP_LIB_TARGET ${PROJECT_NAME})
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/cmake/uisedemo.cmake)
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
    build/unix-deploy.sh
    build/macos-deploy
    build/linux-deploy
    build/windows-deploy.bat
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

