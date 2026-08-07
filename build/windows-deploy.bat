@ECHO OFF
setlocal EnableDelayedExpansion

rem ---------------------------------------------------------------------
rem Builds the uise-desktop demo applications in deployable form and packs
rem them into a single uise-demos-<version>-windows-x64.zip archive.
rem Companion to build/unix-deploy.sh (macos-deploy / linux-deploy).
rem
rem Usage (from the workspace root, one level above uise-desktop\):
rem   uise-desktop\build\windows-deploy.bat [compiler] [build]
rem
rem Env vars (all optional; matches build/windows-ci.bat and windows-vars.bat
rem where names overlap):
rem   UISE_COMPILER          gcc | msvc                          (default: msvc)
rem   UISE_BUILD             release | debug | minsize_release   (default: release)
rem   BUILD_WORKERS          parallel build jobs                 (default: 4)
rem   QT_HOME                Qt install root (must contain bin\windeployqt.exe)
rem   DEPS_UNIVERSAL_ROOT    root of prebuilt deps (Boost etc), same default as windows-vars.bat
rem   UISE_BUILD_DIR         cmake build dir      (default: %cd%\builds\deploy-%TOOLCHAIN%-%UISE_BUILD%)
rem   UISE_DEPLOY_DIR        output dir for the zip archive (default: %cd%\deploy)
rem   UISE_DEPLOY_VERSION    version string for the output file name (default: parsed from CMakeLists.txt)
rem   UISE_DEPLOY_SKIP_BUILD "yes" -> reuse an existing UISE_BUILD_DIR instead of configuring+building
rem ---------------------------------------------------------------------

IF "%UISE_COMPILER%"=="" SET "UISE_COMPILER=%~1"
IF "%UISE_BUILD%"=="" SET "UISE_BUILD=%~2"
IF "%UISE_COMPILER%"=="" SET "UISE_COMPILER=msvc"
IF "%UISE_BUILD%"=="" SET "UISE_BUILD=release"

IF "%DEPS_UNIVERSAL_ROOT%"=="" SET "DEPS_UNIVERSAL_ROOT=C:\projects\dracosha\deps"
IF "%BUILD_WORKERS%"=="" SET "BUILD_WORKERS=4"

IF "%UISE_COMPILER%" == "gcc" (
    CALL %~dp0mingw-config.bat
    SET "TOOLCHAIN=mingw"
) ELSE (
    CALL %~dp0msvc-config.bat
)

IF "%UISE_BUILD%" == "release" SET "BUILD_TYPE=Release"
IF "%UISE_BUILD%" == "debug" SET "BUILD_TYPE=Debug"
IF "%UISE_BUILD%" == "minsize_release" SET "BUILD_TYPE=MinSizeRel"

SET "SRC_DIR=%~dp0.."

IF "%UISE_BUILD_DIR%"=="" SET "UISE_BUILD_DIR=%cd%\builds\deploy-%TOOLCHAIN%-%UISE_BUILD%"
IF "%UISE_DEPLOY_DIR%"=="" SET "UISE_DEPLOY_DIR=%cd%\deploy"

IF "%UISE_DEPLOY_VERSION%"=="" (
    FOR /F "usebackq tokens=3 delims= )" %%V IN (`FINDSTR /R "PROJECT(uisedesktop VERSION" "%SRC_DIR%\CMakeLists.txt"`) DO SET "UISE_DEPLOY_VERSION=%%V"
)
IF "%UISE_DEPLOY_VERSION%"=="" SET "UISE_DEPLOY_VERSION=0.0.0"

ECHO build_dir=%UISE_BUILD_DIR%
ECHO deploy_dir=%UISE_DEPLOY_DIR%
ECHO version=%UISE_DEPLOY_VERSION%

IF NOT EXIST "%UISE_DEPLOY_DIR%" mkdir "%UISE_DEPLOY_DIR%"

IF NOT "%UISE_DEPLOY_SKIP_BUILD%" == "yes" (

    IF EXIST "%UISE_BUILD_DIR%" rmdir /Q /S "%UISE_BUILD_DIR%"
    mkdir "%UISE_BUILD_DIR%"
    SET "CURRENT_DIR=%CD%"
    cd "%UISE_BUILD_DIR%"

    IF "%UISE_COMPILER%" == "gcc" (
        cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DUISE_DESKTOP_DEMO=ON -DUISE_DESKTOP_TEST=OFF -DUISE_DESKTOP_DEMO_BUNDLE=ON "%SRC_DIR%"
        if %errorlevel% neq 0 exit /b %errorlevel%
        cmake --build . -j%BUILD_WORKERS%
        if %errorlevel% neq 0 exit /b %errorlevel%
    ) ELSE (
        call "%MSVCARGS%" %MSVC_ARCH%
        if %errorlevel% neq 0 exit /b %errorlevel%
        cmake -A %MSVC_BUILD_ARCH% -T %MSVC_TOOLSET% -DUISE_DESKTOP_DEMO=ON -DUISE_DESKTOP_TEST=OFF -DUISE_DESKTOP_DEMO_BUNDLE=ON "%SRC_DIR%"
        if %errorlevel% neq 0 exit /b %errorlevel%
        cmake --build . --config %BUILD_TYPE% -- /m:1 /p:UseMultiToolTask=true /p:MultiProcMaxCount=%BUILD_WORKERS%
        if %errorlevel% neq 0 exit /b %errorlevel%
    )

    cd "%CURRENT_DIR%"
)

rem demo/CMakeLists.txt (via cmake/uisedemo.cmake) explicitly sets both
rem RUNTIME_OUTPUT_DIRECTORY and its per-config variants for every demo and
rem for the manager, so all of them land flat in demo\bin regardless of
rem generator. Only the main uisedesktop/ZXing DLLs (no explicit output dir)
rem fall under a \<Config> subdirectory with the MSVC multi-config generator.
SET "DEMO_BIN_DIR=%UISE_BUILD_DIR%\demo\bin"

IF NOT EXIST "%DEMO_BIN_DIR%\uise-demo-manager.exe" (
    ECHO uise-demo-manager.exe not found at %DEMO_BIN_DIR%\uise-demo-manager.exe -- was the build configured with -DUISE_DESKTOP_DEMO_BUNDLE=ON?
    exit /b 1
)

SET "LIB_BIN_DIR=%UISE_BUILD_DIR%"
IF EXIST "%UISE_BUILD_DIR%\%BUILD_TYPE%" SET "LIB_BIN_DIR=%UISE_BUILD_DIR%\%BUILD_TYPE%"

SET "PACKAGE_NAME=uise-demos-%UISE_DEPLOY_VERSION%-windows-x64"
SET "STAGE_DIR=%UISE_BUILD_DIR%\deploy-stage\%PACKAGE_NAME%"
IF EXIST "%STAGE_DIR%" rmdir /Q /S "%STAGE_DIR%"
mkdir "%STAGE_DIR%"

copy /Y "%DEMO_BIN_DIR%\*.exe" "%STAGE_DIR%\" >NUL
copy /Y "%LIB_BIN_DIR%\*.dll" "%STAGE_DIR%\" >NUL 2>NUL

rem windeployqt only understands --debug/--release(-with-debug-info), unlike
rem the three-way UISE_BUILD; minsize_release is still a non-debug build, so
rem it maps to --release same as release.
SET "WINDEPLOYQT_FLAG=--release"
IF "%UISE_BUILD%" == "debug" SET "WINDEPLOYQT_FLAG=--debug"

rem windeployqt de-duplicates as it goes, so running it once per exe against
rem the same target dir is enough to pick up every demo's extra Qt modules
rem (e.g. Multimedia for qrcodescanner-demo) and plugins into one shared tree.
FOR %%E IN ("%STAGE_DIR%\*.exe") DO (
    "%QT_HOME%\bin\windeployqt.exe" %WINDEPLOYQT_FLAG% --no-translations --dir "%STAGE_DIR%" "%%E"
    if %errorlevel% neq 0 exit /b %errorlevel%
)

SET "ZIP_PATH=%UISE_DEPLOY_DIR%\%PACKAGE_NAME%.zip"
IF EXIST "%ZIP_PATH%" del /Q "%ZIP_PATH%"
powershell -NoProfile -Command "Compress-Archive -Path '%STAGE_DIR%\*' -DestinationPath '%ZIP_PATH%' -Force"
if %errorlevel% neq 0 exit /b %errorlevel%

ECHO Created %ZIP_PATH%
