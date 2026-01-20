# Setup dependencies for tedit-cosmo (Windows)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir

Write-Host "Setting up tedit-cosmo dependencies..." -ForegroundColor Cyan
Set-Location $ProjectDir

# Clone cimgui
$CimguiDir = Join-Path $ProjectDir "deps\cimgui\imgui"
if (-not (Test-Path $CimguiDir)) {
    Write-Host "Cloning cimgui..."
    if (Test-Path "deps\cimgui") {
        Remove-Item -Recurse -Force "deps\cimgui"
    }
    git clone --recursive https://github.com/ludoplex/cimgui.git deps/cimgui
} else {
    Write-Host "cimgui already present"
}

Write-Host ""
Write-Host "Setup complete!" -ForegroundColor Green
Write-Host "Build with: make cli  (CLI-only, portable)" -ForegroundColor Yellow
Write-Host "Build with: make gui  (GUI with cimgui)" -ForegroundColor Yellow

