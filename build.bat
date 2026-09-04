@echo off
setlocal

echo ============================================
echo   LEGION Y720 COOLING CONTROLLER BUILD
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

if not exist "%SRC%\Y720CoolingController.c" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingController.c
    pause
    exit /b 1
)

if not exist "%SRC%\Y720CoolingControllerGUI.c" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingControllerGUI.c
    pause
    exit /b 1
)

if not exist "%SRC%\Y720CoolingController.h" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingController.h
    pause
    exit /b 1
)

if not exist "%SRC%\Y720CoolingController.rc" (
    echo ERROR: Missing:
    echo %SRC%\Y720CoolingController.rc
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

echo [1/5] Compiling Y720CoolingController.c...

"%GCC%" ^
    -c "%SRC%\Y720CoolingController.c" ^
    -o "%BUILD%\Y720CoolingController.o" ^
    -I"%SRC%"

if errorlevel 1 (
    echo.
    echo ERROR: Y720CoolingController.c compilation failed.
    pause
    exit /b 1
)

echo OK.
echo.

REM ------------------------------------------------
REM Compile Utils
REM ------------------------------------------------

echo [2/5] Compiling utils.c...

"%GCC%" ^
    -c "%SRC%\utils.c" ^
    -o "%BUILD%\utils.o" ^
    -I"%SRC%"

if errorlevel 1 (
    echo.
    echo ERROR: utils.c compilation failed.
    pause
    exit /b 1
)

echo OK.
echo.

REM ------------------------------------------------
REM Compile GUI
REM ------------------------------------------------

echo [3/5] Compiling Y720CoolingControllerGUI.c...

"%GCC%" ^
    -c "%SRC%\Y720CoolingControllerGUI.c" ^
    -o "%BUILD%\Y720CoolingControllerGUI.o" ^
    -I"%SRC%"

if errorlevel 1 (
    echo.
    echo ERROR: Y720CoolingControllerGUI.c compilation failed.
    pause
    exit /b 1
)

echo OK.
echo.

REM ------------------------------------------------
REM Compile Windows resource
REM ------------------------------------------------

echo [4/5] Compiling Windows resource...

"%WINDRES%" ^
    "%SRC%\Y720CoolingController.rc" ^
    -O coff ^
    -o "%BUILD%\Y720CoolingController_res.o"

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

echo [5/5] Linking executable...

"%GCC%" ^
    "%BUILD%\Y720CoolingControllerGUI.o" ^
    "%BUILD%\Y720CoolingController.o" ^
    "%BUILD%\utils.o" ^
    "%BUILD%\Y720CoolingController_res.o" ^
    -o "%BUILD%\Y720CoolingController.exe" ^
    -municode ^
    -mwindows ^
    -lole32 ^
    -loleaut32 ^
    -lwbemuuid ^
    -luuid ^
    -ladvapi32 ^
    -luser32 ^
    -lshell32 ^
    -lgdi32 ^
    -lcomctl32 ^
    -luxtheme

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
echo %BUILD%\Y720CoolingController.exe
echo.

dir "%BUILD%\Y720CoolingController.exe"

echo.
pause
exit /b 0