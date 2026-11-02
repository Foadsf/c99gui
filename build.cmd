@echo off
rem Build all four c99gui programs with MSVC (cl.exe).
rem Finds MSVC via vswhere if cl is not on PATH.
rem Output: build\c99gui-tui.exe, build\c99gui-pixel.exe, build\c99gui-vector.exe, build\c99gui-suite.exe
rem
rem   build.cmd            optimised, warnings-as-errors, static CRT
rem   build.cmd headless   same, with -DC99GUI_HEADLESS
setlocal
set "ROOT=%~dp0"
if not exist "%ROOT%build" mkdir "%ROOT%build"
set "DEFS="
if /I "%~1"=="headless" set "DEFS=/DC99GUI_HEADLESS"

rem Try cl first (might already be on PATH from Developer Command Prompt)
where cl >nul 2>nul
if %ERRORLEVEL% EQU 0 goto :build

rem Find MSVC via vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo error: vswhere.exe not found - open a Developer Command Prompt or install Visual Studio
    exit /b 1
)

for /f "delims=" %%i in ('"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set "VSINSTALLDIR=%%i"
if not defined VSINSTALLDIR (
    echo error: no Visual Studio with C++ tools found
    exit /b 1
)

set "VCVARS=%VSINSTALLDIR%VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" (
    echo error: vcvarsall.bat not found at %VCVARS%
    exit /b 1
)

call "%VCVARS%" amd64 >nul 2>&1

:build
where cl >nul 2>nul || (
    echo error: cl.exe not found after running vcvarsall.bat
    exit /b 1
)

echo Building c99gui-tui...
cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 %DEFS% /Fo"%ROOT%build\\" /Fe"%ROOT%build\c99gui-tui.exe" "%ROOT%src\tui\c99gui.c"
if errorlevel 1 exit /b 1

echo Building c99gui-pixel...
cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 %DEFS% /Fo"%ROOT%build\\" /Fe"%ROOT%build\c99gui-pixel.exe" "%ROOT%src\pixel\gui.c"
if errorlevel 1 exit /b 1

echo Building c99gui-vector...
cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 %DEFS% /Fo"%ROOT%build\\" /Fe"%ROOT%build\c99gui-vector.exe" "%ROOT%src\vector\gui.c"
if errorlevel 1 exit /b 1

echo Building c99gui-suite...
cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 %DEFS% /Fo"%ROOT%build\\" /Fe"%ROOT%build\c99gui-suite.exe" "%ROOT%src\suite\gui.c"
if errorlevel 1 exit /b 1

echo All four programs built successfully.
exit /b 0
