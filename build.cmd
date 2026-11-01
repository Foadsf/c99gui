@echo off
rem Build c99gui with MSVC (cl.exe) or clang-cl. Run from a Developer Command
rem Prompt (or any prompt where cl is on PATH). Output: build\gui.exe
rem   build.cmd            optimised, warnings-as-errors, static CRT
rem   build.cmd headless   same, with -DC99GUI_HEADLESS
setlocal
set "ROOT=%~dp0"
if not exist "%ROOT%build" mkdir "%ROOT%build"
set "DEFS="
set "OUT=gui.exe"
if /I "%~1"=="headless" ( set "DEFS=/DC99GUI_HEADLESS" & set "OUT=gui-headless.exe" )
where cl >nul 2>nul || ( echo error: cl.exe not found - open a Developer Command Prompt & exit /b 1 )
cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 %DEFS% /Fo"%ROOT%build\\" /Fe"%ROOT%build\%OUT%" "%ROOT%src\gui.c"
exit /b %ERRORLEVEL%
