@echo off
setlocal
cd /d "%~dp0"
if "%HTTP_SERVER_PORT%"=="" set "HTTP_SERVER_PORT=8081"
if "%OPCUA_SERVER_PORT%"=="" set "OPCUA_SERVER_PORT=4841"
if not exist "%~dp0MonitoringRuntime.exe" (
  echo MonitoringRuntime.exe was not found in:
  echo %~dp0
  pause
  exit /b 1
)

echo Starting Monitoring Runtime in a separate window...
start "Monitoring Runtime" /D "%~dp0" "%~dp0MonitoringRuntime.exe"
echo A separate "Monitoring Runtime" window has opened.
echo Keep that window open while using the platform, then open:
echo http://127.0.0.1:%HTTP_SERVER_PORT%
pause