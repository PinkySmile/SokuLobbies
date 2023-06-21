@echo off
set /p port=Enter server port: 
set /p slots=Enter server max slots: 
set /p name=Enter lobby name: 
set /p name=Enter lobby password: 
SokuLobbiesServer.exe "%port%" "%slots%" "%name%" "%pwd%"
pause