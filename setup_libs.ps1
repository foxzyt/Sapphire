# PowerShell script to download and extract pre-compiled dependencies for Sapphire
$ErrorActionPreference = "Stop"

$libsUrl = "https://github.com/foxzyt/Sapphire/releases/download/v1.0.7/libs.zip"
$zipPath = Join-Path $PSScriptRoot "libs.zip"
$destFolder = Join-Path $PSScriptRoot "libs"

if (Test-Path $destFolder) {
    Write-Host "The 'libs' folder already exists. If you want to re-download, delete it first." -ForegroundColor Yellow
    Exit
}

Write-Host "Downloading dependencies from $libsUrl..." -ForegroundColor Cyan
try {
    Invoke-WebRequest -Uri $libsUrl -OutFile $zipPath -UserAgent "Mozilla/5.0"
} catch {
    Write-Host "Failed to download libraries. Please check the URL or your internet connection." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Exit
}

Write-Host "Extracting libraries to $destFolder..." -ForegroundColor Cyan
try {
    Expand-Archive -Path $zipPath -DestinationPath $PSScriptRoot -Force
    Write-Host "Extraction completed successfully!" -ForegroundColor Green
} catch {
    Write-Host "Failed to extract the ZIP file." -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
} finally {
    if (Test-Path $zipPath) {
        Remove-Item $zipPath -Force
    }
}
