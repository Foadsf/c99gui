@echo off
rem Build c99gui with MSVC (cl.exe) or clang-cl. Run from a Developer Command
rem Prompt (or any prompt where cl is on PATH). Output: build\c99gui.exe
rem   build.cmd            optimised, warnings-as-errors, static CRT
rem   build.cmd strict     same, with -DSTRICT_C99
setlocal
set "ROOT=%~dp0"
if not exist "%ROOT%build" mkdir "%ROOT%build"
set "DEFS="
set "OUT=c99gui.exe"
if /I "%~1"=="strict" ( set "DEFS=/DSTRICT_C99" & set "OUT=c99gui-strict.exe" )
where cl >nul 2>nul || ( echo error: cl.exe not found - open a Developer Command Prompt & exit /b 1 )
cl /nologo /std:c11 /W4 /WX /O2 /MT /utf-8 %DEFS% /Fo"%ROOT%build\\" /Fe"%ROOT%build\%OUT%" "%ROOT%src\c99gui.c"
exit /b %ERRORLEVEL%
