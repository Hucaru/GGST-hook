@echo off

set VSTOOLS="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
call %VSTOOLS%

set FLAGS=/Fe:ggs_hook.dll /std:c++latest /EHsc
set LIBS=User32.lib Kernel32.lib detours.lib Wininet.lib Winhttp.lib

cl.exe /LD src/main.cpp src/async.cpp %LIBS% %FLAGS%