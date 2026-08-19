@echo off
setlocal

echo ============================================
echo   LEGION Y720 COOLING MONITOR BUILD
echo ============================================
echo.

REM ------------------------------------------------
REM Paths
REM ------------------------------------------------

set ROOT=%~dp0
set SRC=%ROOT%src
set BUILD=%ROOT%build

set GCC=C:\msys64\ucrt64\bin\gcc.exe
set WINDRES=C:\msys64\ucrt64\bin\windres.exe

REM ------------------------------------------------
REM Check tools
REM ------------------------------------------------

if not exist "%GCC%" (
    echo ERROR: GCC not found:
    echo %GCC%
    pause
    exit /b 1
)

if not exist "%WINDRES%" (
    echo ERROR: windres not found:
    echo %WINDRES%
    pause
    exit /b 1
)

REM ------------------------------------------------
REM Check source files
REM ------------------------------------------------

if not exist "%SRC%\Y720CoolingMonitor.c" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingMonitor.c
    pause
    exit /b 1
)

if not exist "%SRC%\Y720CoolingMonitorGUI.c" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingMonitorGUI.c
    pause
    exit /b 1
)

if not exist "%SRC%\Y720CoolingMonitor.h" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingMonitor.h
    pause
    exit /b 1
)

if not exist "%SRC%\Y720CoolingMonitor.rc" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingMonitor.rc
    pause
    exit /b 1
)

REM ------------------------------------------------
REM Clean build directory
REM ------------------------------------------------

echo Cleaning build directory...

if exist "%BUILD%" (
    del /q "%BUILD%\*" >nul 2>&1
) else (
    mkdir "%BUILD%"
)

echo.

REM ------------------------------------------------
REM Compile GameZone / WMI
REM ------------------------------------------------

echo [1/4] Compiling Y720CoolingMonitor.c...

"%GCC%" ^
    -c "%SRC%\Y720CoolingMonitor.c" ^
    -o "%BUILD%\Y720CoolingMonitor.o" ^
    -I"%SRC%"

if errorlevel 1 (
    echo.
    echo ERROR: Y720CoolingMonitor.c compilation failed.
    pause
    exit /b 1
)

echo OK.
echo.

REM ------------------------------------------------
REM Compile GUI
REM ------------------------------------------------

echo [2/4] Compiling Y720CoolingMonitorGUI.c...

"%GCC%" ^
    -c "%SRC%\Y720CoolingMonitorGUI.c" ^
    -o "%BUILD%\Y720CoolingMonitorGUI.o" ^
    -I"%SRC%"

if errorlevel 1 (
    echo.
    echo ERROR: Y720CoolingMonitorGUI.c compilation failed.
    pause
    exit /b 1
)

echo OK.
echo.

REM ------------------------------------------------
REM Compile Windows resource
REM ------------------------------------------------

echo [3/4] Compiling Windows resource...

"%WINDRES%" ^
    "%SRC%\Y720CoolingMonitor.rc" ^
    -O coff ^
    -o "%BUILD%\Y720CoolingMonitor_res.o"

if errorlevel 1 (
    echo.
    echo ERROR: resource compilation failed.
    pause
    exit /b 1
)

echo OK.
echo.

REM ------------------------------------------------
REM Link executable
REM ------------------------------------------------

echo [4/4] Linking executable...

"%GCC%" ^
    "%BUILD%\Y720CoolingMonitorGUI.o" ^
    "%BUILD%\Y720CoolingMonitor.o" ^
    "%BUILD%\Y720CoolingMonitor_res.o" ^
    -o "%BUILD%\Y720CoolingMonitor.exe" ^
    -municode ^
    -mwindows ^
    -lole32 ^
    -loleaut32 ^
    -lwbemuuid ^
    -luuid ^
    -ladvapi32 ^
    -luser32 ^
    -lgdi32

if errorlevel 1 (
    echo.
    echo ERROR: Linking failed.
    pause
    exit /b 1
)

echo.
echo ============================================
echo   BUILD SUCCESSFUL
echo ============================================
echo.
echo Executable:
echo %BUILD%\Y720CoolingMonitor.exe
echo.

dir "%BUILD%\Y720CoolingMonitor.exe"

echo.
pause
exit /b 0