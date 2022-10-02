@echo off
set /p port=Enter server port: 
set /p slots=Enter server max slots: 
set /p name=Enter lobby name: 
SokuLobbiesServer.exe "%port%" "%slots%" "%name%"
pause