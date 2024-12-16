@echo off
c:\RTEdbg\UTIL\Extract_msg\Extract_msg.exe messages.h x64\Debug\Messages.txt
xcopy x64\Debug\Messages.txt c:\RTEdbg\RTEmsg /y >nul
xcopy x64\ReleaseWithDLL\RTEmsg.exe c:\RTEdbg\RTEmsg /y >nul
