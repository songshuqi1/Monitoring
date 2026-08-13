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