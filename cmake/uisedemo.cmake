#
# Shared helper for building demo/<name> applications.
#
# By default demos are plain executables (fast to build, no deploy tooling
# required). Set UISE_DESKTOP_DEMO_BUNDLE=ON to build them in a form that can
# be packaged by build/unix-deploy.sh and build/windows-deploy.bat:
#   - macOS: every demo executable is placed next to the demo manager's
#            app bundle executable (single shared bundle, single Qt payload)
#   - Windows: demos are built as WIN32_EXECUTABLE (no console window)
#   - Linux: no change, demos are already plain executables
#
# All demos land in one directory (UISE_DEMO_BIN_DIR) regardless of the mode,
# so the demo manager can always find sibling demo executables next to
# itself both in a developer build and in a deployed package.
#

OPTION(UISE_DESKTOP_DEMO_BUNDLE "Build demo applications in deployable app-bundle form" OFF)

SET(UISE_DEMO_BIN_DIR ${CMAKE_BINARY_DIR}/demo/bin CACHE INTERNAL "Common output directory for demo executables")
SET(UISE_DEMO_MANAGER_NAME "uise-demo-manager" CACHE INTERNAL "Target/bundle name of the demo manager application")

SET_PROPERTY(GLOBAL PROPERTY UISE_DEMO_REGISTRY "")

# uise_demo(
#     NAME <target>
#     TITLE <short title>
#     DESCRIPTION <one-line description>
#     SOURCES <source files...>
#     [HEADERS <header files...>]
#     [RESOURCES <.qrc files...>]
#     [MACOS_INFO_PLIST <Info.plist.in>]
#     [PRIORITY <single digit, default 5>]
# )
# PRIORITY controls ordering in the demo manager's list: entries are sorted
# by PRIORITY first, then alphabetically by TITLE within the same PRIORITY.
# Lower sorts first (0 = pinned to the very top, 9 = pinned to the bottom).
FUNCTION(uise_demo)

    SET(oneValueArgs NAME TITLE DESCRIPTION MACOS_INFO_PLIST PRIORITY)
    SET(multiValueArgs SOURCES HEADERS RESOURCES)
    CMAKE_PARSE_ARGUMENTS(ARG "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    IF (NOT ARG_NAME)
        MESSAGE(FATAL_ERROR "uise_demo: NAME is required")
    ENDIF()
    IF (NOT ARG_TITLE)
        SET(ARG_TITLE ${ARG_NAME})
    ENDIF()
    # DEFINED, not a truthiness check: CMake's if() treats the string "0" as
    # boolean false, so `IF (NOT ARG_PRIORITY)` would wrongly reset an
    # explicit PRIORITY 0 back to the default.
    IF (NOT DEFINED ARG_PRIORITY)
        SET(ARG_PRIORITY "5")
    ENDIF()
    IF (NOT ARG_PRIORITY MATCHES "^[0-9]$")
        MESSAGE(FATAL_ERROR "uise_demo(${ARG_NAME}): PRIORITY must be a single digit 0-9")
    ENDIF()

    SET(demoSources ${ARG_HEADERS} ${ARG_SOURCES})
    IF (ARG_RESOURCES)
        QT6_ADD_RESOURCES(demoSources ${ARG_RESOURCES})
    ENDIF()

    ADD_EXECUTABLE(${ARG_NAME} ${demoSources})
    TARGET_LINK_LIBRARIES(${ARG_NAME} PRIVATE ${UISE_DESKTOP_LIB_TARGET})

    IF (WIN32 AND UISE_DESKTOP_DEMO_BUNDLE)
        SET_TARGET_PROPERTIES(${ARG_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
    ENDIF()

    IF (APPLE AND UISE_DESKTOP_DEMO_BUNDLE)
        SET(demoOutDir ${UISE_DEMO_BIN_DIR}/${UISE_DEMO_MANAGER_NAME}.app/Contents/MacOS)
    ELSE()
        SET(demoOutDir ${UISE_DEMO_BIN_DIR})
    ENDIF()

    SET_TARGET_PROPERTIES(${ARG_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${demoOutDir})
    FOREACH (cfg Debug Release RelWithDebInfo MinSizeRel)
        STRING(TOUPPER ${cfg} cfgUpper)
        SET_TARGET_PROPERTIES(${ARG_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_${cfgUpper} ${demoOutDir})
    ENDFOREACH()

    # A plain (non-bundle) macOS executable can still carry Info.plist keys
    # (e.g. NSCameraUsageDescription) embedded via a linker section, so
    # per-demo OS permission prompts keep working outside of bundle mode too.
    IF (APPLE AND ARG_MACOS_INFO_PLIST AND NOT UISE_DESKTOP_DEMO_BUNDLE)
        SET(MACOSX_BUNDLE_BUNDLE_NAME ${ARG_TITLE})
        SET(MACOSX_BUNDLE_GUI_IDENTIFIER "org.uise.desktop.demo.${ARG_NAME}")
        SET(MACOSX_BUNDLE_EXECUTABLE_NAME ${ARG_NAME})
        SET(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
        SET(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
        SET(MACOSX_BUNDLE_LONG_VERSION_STRING ${PROJECT_VERSION})
        SET(MACOSX_BUNDLE_INFO_STRING ${ARG_TITLE})
        SET(MACOSX_BUNDLE_COPYRIGHT "")
        SET(MACOSX_BUNDLE_ICON_FILE "")

        SET(plistOut ${CMAKE_CURRENT_BINARY_DIR}/${ARG_NAME}-Info.plist)
        CONFIGURE_FILE(${ARG_MACOS_INFO_PLIST} ${plistOut})
        TARGET_LINK_OPTIONS(${ARG_NAME} PRIVATE "LINKER:-sectcreate,__TEXT,__info_plist,${plistOut}")
    ENDIF()

    # "|" is the field separator and ";" is CMake's own list separator when
    # this entry is later read back with FOREACH() in demo/CMakeLists.txt --
    # either one in TITLE/DESCRIPTION would corrupt or truncate every demo's
    # registry entry, so reject it here with a clear, per-demo error instead.
    IF (ARG_TITLE MATCHES "[|;]" OR ARG_DESCRIPTION MATCHES "[|;]")
        MESSAGE(FATAL_ERROR "uise_demo(${ARG_NAME}): TITLE/DESCRIPTION must not contain '|' or ';'")
    ENDIF()

    # Registry entries are sorted as plain strings, so PRIORITY then TITLE go first.
    SET_PROPERTY(GLOBAL APPEND PROPERTY UISE_DEMO_REGISTRY "${ARG_PRIORITY}|${ARG_TITLE}|${ARG_NAME}|${ARG_DESCRIPTION}")

ENDFUNCTION()
