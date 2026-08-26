param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$FrontendDir = Join-Path $Root "frontend"
$BackendDir = Join-Path $Root "backend"
$BackendBuildDir = Join-Path $Root "backend\build-runtime"
$ReleaseDir = Join-Path $Root "release\MonitoringRuntime"

$VsDevCmdCandidates = @(
    (Join-Path $env:ProgramFiles "Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"),
    (Join-Path $env:ProgramFiles "Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat")
)
$VsDevCmd = $VsDevCmdCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $VsDevCmd) {
    throw "Visual Studio C++ build tools were not found. Install Desktop development with C++."
}

$PythonCandidates = @(
    (Join-Path $env:LocalAppData "Programs\Python\Python310\python.exe"),
    (Join-Path $env:LocalAppData "Programs\Python\Python311\python.exe"),
    (Join-Path $env:LocalAppData "Programs\Python\Python312\python.exe"),
    (Join-Path $env:LocalAppData "Programs\Python\Python313\python.exe")
)
$PythonExe = $PythonCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $PythonExe) {
    throw "Python 3 is required by open62541. Install Python and run this script again."
}

Write-Host "Building frontend..."
Push-Location $FrontendDir
try {
    npm.cmd run build
    if ($LASTEXITCODE -ne 0) {
        throw "Frontend build failed with exit code $LASTEXITCODE"
    }
} finally {
    Pop-Location
}

Write-Host "Embedding frontend assets..."
& (Join-Path $PSScriptRoot "embed_frontend.ps1")

Write-Host "Building backend ($Configuration)..."
New-Item -ItemType Directory -Force -Path $BackendBuildDir | Out-Null
$ConfigureCommand = 'call "{0}" -arch=x64 -host_arch=x64 && cmake -S "{1}" -B "{2}" -G Ninja -DCMAKE_BUILD_TYPE={3} -DPYTHON_EXECUTABLE="{4}" -UMYSQL_LIB -UMYSQL_INCLUDE_DIR' -f $VsDevCmd, $BackendDir, $BackendBuildDir, $Configuration, $PythonExe
& cmd.exe /d /s /c $ConfigureCommand
if ($LASTEXITCODE -ne 0) {
    throw "Backend configure failed with exit code $LASTEXITCODE"
}

$BuildCommand = 'call "{0}" -arch=x64 -host_arch=x64 && cmake --build "{1}" --parallel' -f $VsDevCmd, $BackendBuildDir
& cmd.exe /d /s /c $BuildCommand
if ($LASTEXITCODE -ne 0) {
    throw "Backend build failed with exit code $LASTEXITCODE"
}

$ExePath = Join-Path $BackendBuildDir "$Configuration\MonitoringPlatform.exe"
if (-not (Test-Path $ExePath)) {
    $ExePath = Join-Path $BackendBuildDir "MonitoringPlatform.exe"
}
if (-not (Test-Path $ExePath)) {
    throw "Backend executable not found: $ExePath"
}

New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
Copy-Item $ExePath (Join-Path $ReleaseDir "MonitoringRuntime.exe") -Force

$MysqlDllCandidates = @(
    "C:\Program Files\MySQL\MySQL Server 9.7\lib\libmysql.dll",
    "C:\Program Files\MySQL\MySQL Server 8.0\lib\libmysql.dll",
    "C:\Program Files\MySQL\MySQL Server 5.7\lib\libmysql.dll"
)
$MysqlDll = $MysqlDllCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if ($MysqlDll) {
    Copy-Item $MysqlDll (Join-Path $ReleaseDir "libmysql.dll") -Force
}

$StartBat = @"
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
"@
[System.IO.File]::WriteAllText((Join-Path $ReleaseDir "start_runtime.bat"), $StartBat, [System.Text.Encoding]::ASCII)

$PublicStartPs1 = @'
$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $Root

if ([string]::IsNullOrWhiteSpace($env:HTTP_SERVER_PORT)) {
    $env:HTTP_SERVER_PORT = "8081"
}
if ([string]::IsNullOrWhiteSpace($env:OPCUA_SERVER_PORT)) {
    $env:OPCUA_SERVER_PORT = "4841"
}

$Runtime = Join-Path $Root "MonitoringRuntime.exe"
$Cloudflared = Join-Path $Root "cloudflared.exe"
$LogFile = Join-Path $Root "cloudflared-public.log"

function Get-ListenPortOwner {
    param([int]$Port)
    try {
        Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction Stop | Select-Object -First 1
    } catch {
        $null
    }
}

if (-not (Test-Path $Runtime)) {
    Write-Host "MonitoringRuntime.exe was not found." -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

if (-not (Test-Path $Cloudflared)) {
    $Command = Get-Command cloudflared.exe -ErrorAction SilentlyContinue
    if ($Command) {
        $Cloudflared = $Command.Source
    } else {
        Write-Host "cloudflared.exe was not found." -ForegroundColor Red
        Write-Host "Download cloudflared for Windows and put cloudflared.exe in this folder:"
        Write-Host $Root
        Write-Host "https://developers.cloudflare.com/cloudflare-one/connections/connect-networks/downloads/"
        Read-Host "Press Enter to exit"
        exit 1
    }
}

Write-Host "Monitoring Runtime starting on local port $env:HTTP_SERVER_PORT..."
$PortOwner = Get-ListenPortOwner ([int]$env:HTTP_SERVER_PORT)
if ($PortOwner) {
    $OwnerProcess = Get-Process -Id $PortOwner.OwningProcess -ErrorAction SilentlyContinue
    $OwnerPath = if ($OwnerProcess) { $OwnerProcess.Path } else { "" }
    if ($OwnerProcess -and ($OwnerProcess.ProcessName -eq "MonitoringRuntime" -or $OwnerProcess.ProcessName -eq "MonitoringPlatform")) {
        Write-Host "Monitoring service is already listening on port $env:HTTP_SERVER_PORT (PID $($OwnerProcess.Id)). Reusing it so active communication resources stay connected." -ForegroundColor Yellow
    } else {
        Write-Host "Port $env:HTTP_SERVER_PORT is occupied by PID $($PortOwner.OwningProcess). Stop that program or set HTTP_SERVER_PORT to another port." -ForegroundColor Red
        Read-Host "Press Enter to exit"
        exit 1
    }
} else {
    Start-Process -FilePath $Runtime -WorkingDirectory $Root -WindowStyle Normal
}
Start-Sleep -Seconds 3

$LocalUrl = "http://127.0.0.1:$env:HTTP_SERVER_PORT"
try {
    Invoke-WebRequest -UseBasicParsing $LocalUrl -TimeoutSec 5 | Out-Null
    Write-Host "Local service is ready: $LocalUrl" -ForegroundColor Green
} catch {
    Write-Host "Local service is not ready yet. Cloudflared will still try to connect." -ForegroundColor Yellow
}

"" | Set-Content -Path $LogFile -Encoding UTF8
Write-Host ""
Write-Host "Starting public Cloudflare tunnel..."
Write-Host "Waiting for public URL. Log file: $LogFile"
Write-Host ""

$FoundUrl = $null
$OldErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
& $Cloudflared tunnel --no-autoupdate --url $LocalUrl 2>&1 | ForEach-Object {
    $Line = $_.ToString()
    Add-Content -Path $LogFile -Value $Line -Encoding UTF8
    Write-Host $Line

    if (-not $FoundUrl -and $Line -match "https://[-a-zA-Z0-9.]+\.trycloudflare\.com") {
        $FoundUrl = $Matches[0]
        Write-Host ""
        Write-Host "============================================================" -ForegroundColor Cyan
        Write-Host "Public URL:" -ForegroundColor Cyan
        Write-Host $FoundUrl -ForegroundColor Green
        Write-Host "Send this URL to other computers." -ForegroundColor Cyan
        Write-Host "============================================================" -ForegroundColor Cyan
        Write-Host ""
    }
}
$ErrorActionPreference = $OldErrorActionPreference

Write-Host ""
Write-Host "Cloudflared stopped."
if (-not $FoundUrl) {
    Write-Host "No trycloudflare URL was captured. Open cloudflared-public.log and search for trycloudflare.com." -ForegroundColor Yellow
}
Read-Host "Press Enter to exit"
'@
[System.IO.File]::WriteAllText((Join-Path $ReleaseDir "start_public_runtime.ps1"), $PublicStartPs1, [System.Text.Encoding]::UTF8)

$PublicStartBat = @"
@echo off
setlocal
cd /d "%~dp0"
powershell.exe -ExecutionPolicy Bypass -File "%~dp0start_public_runtime.ps1"
pause
"@
[System.IO.File]::WriteAllText((Join-Path $ReleaseDir "start_public_runtime.bat"), $PublicStartBat, [System.Text.Encoding]::ASCII)

$Readme = @"
Monitoring Runtime
==================

This folder is the runtime package. It does not contain frontend/src, backend/src, headers, node_modules, or build scripts.

Run:
  start_runtime.bat

Public internet access without hardware:
  1. Download cloudflared.exe for Windows from Cloudflare.
  2. Put cloudflared.exe in this folder.
  3. Run start_public_runtime.bat.
  4. Share the printed https://*.trycloudflare.com URL with other computers.

Communication resources:
  The public URL only exposes this web/API service on port 8081.
  OPC UA and UDP data sources are still connected by this computer's backend.
  Software-defined communication (SDC) is configured in Communication Resources.
  Enter the Monitor Agent address (for example http://DEVICE-IP:9100); the runtime
  directly polls /api/runtime/stream and expects {"vars":{"name": value}}.
  Enabled SDC resources are saved under data and automatically restart with the runtime.
  For OPC UA, the endpoint must be reachable from this computer, for example:
    opc.tcp://DEVICE-IP:4840
  Cloudflare Quick Tunnel does not expose OPC UA/UDP ports automatically.

Local access:
  http://127.0.0.1:8081

LAN access:
  http://202.118.21.28:8081

Optional ports:
  HTTP_SERVER_PORT controls the web/API port. Default: 8081.
  OPCUA_SERVER_PORT controls the local OPC UA server port. Default: 4841.

Open Windows Firewall port 8081 if another computer cannot access it.
"@
[System.IO.File]::WriteAllText((Join-Path $ReleaseDir "README_RUNTIME.txt"), $Readme, [System.Text.Encoding]::UTF8)

Write-Host "Runtime package created:"
Write-Host $ReleaseDir
