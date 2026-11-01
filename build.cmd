@echo off
rem Build all four c99gui programs with MSVC (cl.exe).
rem Run from a Developer Command Prompt (or any prompt where cl is on PATH).
rem Output: build\c99gui-tui.exe, build\c99gui-pixel.exe, build\c99gui-vector.exe, build\c99gui-suite.exe
rem
rem   build.cmd            optimised, warnings-as-errors, static CRT
rem   build.cmd headless   same, with -DC99GUI_HEADLESS
setlocal
set "ROOT=%~dp0"
if not exist "%ROOT%build" mkdir "%ROOT%build"
set "DEFS="
if /I "%~1"=="headless" set "DEFS=/DC99GUI_HEADLESS"
where cl >nul 2>nul || ( echo error: cl.exe not found - open a Developer Command Prompt & exit /b 1 )

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
