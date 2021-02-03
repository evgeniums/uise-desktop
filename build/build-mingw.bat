@ECHO OFF

SET BOOST_ROOT=C:/projects/dracosha/deps/root-mingw-x86_64
SET QT_HOME=C:/Qt/Qt6/6.0.0/mingw81_64

SET PATH=C:\Program Files\CMake\bin;C:/Programs/msys64/mingw64/bin;C:\projects\dracosha\deps\root-mingw-x86_64\lib;C:\Qt\Qt6\6.0.0\mingw81_64\bin;C:\projects\uise\build\test;%PATH%

cmake -G "MinGW Makefiles" -DUISE_TEST_JUNIT=ON -DCMAKE_BUILD_TYPE=Release C:\projects\uise\uise-desktop
cmake --build . -j8 --target all  
ctest -VV