rem @ECHO OFF

IF "%UISE_COMPILER%"=="" SET UISE_COMPILER=%1
IF "%UISE_BUILD%"=="" SET UISE_BUILD=%2
IF "%UISE_TEST_NAME%"=="" SET UISE_TEST_NAME=%3

IF "%DEPS_UNIVERSAL_ROOT%"=="" SET DEPS_UNIVERSAL_ROOT=C:\projects\dracosha\deps
IF "%BUILD_WORKERS%"=="" SET BUILD_WORKERS=4

IF "%UISE_COMPILER%" == "gcc" (
    call %~dp0mingw-config.bat
) ELSE (
    call %~dp0msvc-config.bat
)

IF "%UISE_BUILD%" == "release" (
SET BUILD_TYPE=Release
)
IF "%UISE_BUILD%" == "debug" (
SET BUILD_TYPE=Debug
)
IF "%UISE_BUILD%" == "minsize_release" (
SET BUILD_TYPE=MinSizeRel
)

SET BUILD_DIR=%cd%\builds\build-%TOOLCHAIN%-%UISE_BUILD%
SET SRC_DIR=%~dp0..

SET PATH=%BUILD_DIR%;%BUILD_DIR%\%BUILD_TYPE%;%QT_HOME%\bin;%BOOST_ROOT%\lib;%EXTRA_PATH%;%PATH%
