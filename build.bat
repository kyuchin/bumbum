@echo off
title Telegram Builder
echo ========================================
echo  Telegram Messenger - Building...
echo ========================================

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Visual Studio environment could not be loaded!
    exit /b 1
)

echo [OK] Visual Studio environment loaded
echo.

set SOURCES=main.cpp MainWindow.cpp TrayManager.cpp ScheduleManager.cpp ConfigManager.cpp Logger.cpp ProcessHelper.cpp Watchdog.cpp

echo Compiling resources...
rc.exe /nologo resource.rc

echo Compiling source files and linking...
cl.exe /nologo /O2 /EHsc /W3 /utf-8 %SOURCES% resource.res /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib comdlg32.lib shell32.lib shlwapi.lib advapi32.lib /OUT:"Telegram.exe"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo  BUILD SUCCESSFUL - Telegram.exe
    echo ========================================
) else (
    echo.
    echo ========================================
    echo  BUILD FAILED! Error code: %ERRORLEVEL%
    echo ========================================
)
