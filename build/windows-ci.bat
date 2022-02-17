rem @ECHO OFF

CALL %~dp0windows-vars.bat %1 %2 %3

IF "%UISE_TEST_NAME%" == "" GOTO test_all
IF "%UISE_TEST_NAME%" == "all" (
    SET UISE_RUN_TESTS=1
    GOTO test_all
)
ECHO Running test %UISE_TEST_NAME%
SET TEST_NAME=-R ^%UISE_TEST_NAME%$
SET UISE_RUN_TESTS=1
GOTO skip_test_all
:test_all
ECHO Running all tests
SET TEST_NAME=-L ALL
:skip_test_all

IF EXIST %BUILD_DIR% rmdir /Q /S %BUILD_DIR%
mkdir %BUILD_DIR%
SET CURRENT_DIR=%CD%
cd %BUILD_DIR%

IF "%UISE_COMPILER%" == "gcc" (

cmake -G "MinGW Makefiles" -DUISE_TEST_JUNIT=ON -DCMAKE_BUILD_TYPE=%BUILD_TYPE% %SRC_DIR%
if %errorlevel% neq 0 exit %errorlevel%

cmake --build . -j%BUILD_WORKERS%
if %errorlevel% neq 0 exit %errorlevel%

IF "%UISE_RUN_TESTS%"=="1" (
ctest -VV %TEST_NAME%
if %errorlevel% neq 0 exit %errorlevel%
)

) else (

call "%MSVCARGS%" %MSVC_ARCH%
if %errorlevel% neq 0 exit %errorlevel%

cmake -A %MSVC_BUILD_ARCH% -T %MSVC_TOOLSET% -DUISE_TEST_JUNIT=ON %SRC_DIR%
if %errorlevel% neq 0 exit %errorlevel%

cmake --build . --config %BUILD_TYPE% -- /m:1 /p:UseMultiToolTask=true /p:MultiProcMaxCount=%BUILD_WORKERS% /fileLogger
if %errorlevel% neq 0 exit %errorlevel%

IF "%UISE_RUN_TESTS%" == "1" (
ctest -C %BUILD_TYPE% -VV %TEST_NAME%
if %errorlevel% neq 0 exit %errorlevel%
)
)

cd %CURRENT_DIR%
