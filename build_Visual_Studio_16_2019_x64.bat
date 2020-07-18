
@echo off
SETLOCAL

set GENERATOR="Visual Studio 16 2019"
set PLATFORM=x64

set CMAKE_BIN_x86="C:\Program Files (x86)\CMake\bin\cmake.exe"
set CMAKE_BIN_x64="C:\Program Files\CMake\bin\cmake.exe"
IF EXIST %CMAKE_BIN_x64% (
	echo CMake 64-bit detected
	set CMAKE_BIN=%CMAKE_BIN_x64%
) ELSE (
	IF EXIST %CMAKE_BIN_x86% (
		echo CMake 32-bit detected
		set CMAKE_BIN=%CMAKE_BIN_x86%
	) ELSE (
		echo Cannot detect either %CMAKE_BIN_x86% or
		echo %CMAKE_BIN_x64% make sure CMake is installed
		EXIT /B 1
	)
)
echo Using CMake at %CMAKE_BIN%

mkdir build
cd build
echo --- Building betsy ---
%CMAKE_BIN% -G %GENERATOR% -A %PLATFORM% ..
%CMAKE_BIN% --build . --config Debug
%CMAKE_BIN% --build . --target install --config Debug
%CMAKE_BIN% --build . --config Release
%CMAKE_BIN% --build . --target install --config Release

echo Done!

ENDLOCAL