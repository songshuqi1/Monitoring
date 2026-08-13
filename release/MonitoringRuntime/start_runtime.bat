@echo off
setlocal
cd /d "%~dp0"
if "%HTTP_SERVER_PORT%"=="" set "HTTP_SERVER_PORT=8081"
if "%OPCUA_SERVER_PORT%"=="" set "OPCUA_SERVER_PORT=4841"
echo Monitoring Runtime starting...
echo Open http://127.0.0.1:%HTTP_SERVER_PORT% on this computer, or http://202.118.21.28:%HTTP_SERVER_PORT% from another computer on the same LAN.
"%~dp0MonitoringRuntime.exe"
echo Monitoring Runtime stopped.
pause