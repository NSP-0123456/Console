@echo off
setlocal

set GCC_PATH=C:\Progra~1\MinGW

if "%1"=="debug" (goto debug) else (goto release)

:release
"%GCC_PATH%\bin\mingw32-make.exe" "GCCPATH=%GCC_PATH%" %1 %2 %3 %4 %5 %6 %7 %8 %9
goto end

:debug
"%GCC_PATH%\bin\mingw32-make.exe" "GCCPATH=%GCC_PATH%" "DEBUG=1" %2 %3 %4 %5 %6 %7 %8 %9
goto end

:end

:cleaning
if EXIST Console.o (del Console.o)
if EXIST Resources.o (del Resources.o)
if EXIST Parser.o (del Parser.o)
rem if EXIST Parser.c (del Parser.c)
rem if EXIST Parser.h (del Parser.h)
rem if EXIST Scanner.c (del Scanner.c)
if EXIST Scanner.o (del Scanner.o)

set GCC_PATH=
endlocal
