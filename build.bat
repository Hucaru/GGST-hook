@echo off

set VSTOOLS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VSTOOLS%

set FLAGS=/Fe:ggs_hook.dll
set LIBS=User32.lib Kernel32.lib detours.lib Wininet.lib

cl.exe /LD main.cpp %LIBS% %FLAGS%