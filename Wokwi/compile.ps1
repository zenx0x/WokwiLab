# compile.ps1
$folderPath = Read-Host "Enter the folder path"
if (-not (Test-Path $folderPath -PathType Container)) {
    Write-Host "Error: Folder does not exist."
    exit 1
}
Set-Location $folderPath -ErrorAction Stop
pio run
if ($LASTEXITCODE -ne 0) {
    Write-Host "Error: Compilation failed."
    exit 1
}
Write-Host "Compilation succeeded."