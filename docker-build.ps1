# Builds the PS4 Linux Launcher .pkg inside the OpenOrbis toolchain container.
#
# Usage (from the repo root):
#   ./docker-build.ps1            # build the toolchain image (if needed) and compile the pkg
#   ./docker-build.ps1 -Rebuild   # force rebuild of the toolchain image
#   ./docker-build.ps1 -Shell     # drop into an interactive shell in the container
[CmdletBinding()]
param(
    [switch]$Rebuild,
    [switch]$Shell
)

$ErrorActionPreference = 'Stop'
$ImageName = 'ps4-linux-launcher-toolchain'
$ProjectDir = $PSScriptRoot

# Ensure the Docker daemon is reachable.
try {
    docker info *> $null
    if ($LASTEXITCODE -ne 0) { throw }
} catch {
    Write-Error "Docker daemon is not reachable. Start Docker Desktop and try again."
    exit 1
}

$buildArgs = @('build', '-t', $ImageName, $ProjectDir)
if ($Rebuild) { $buildArgs += '--no-cache' }
Write-Host "==> Building toolchain image '$ImageName'..." -ForegroundColor Cyan
docker @buildArgs
if ($LASTEXITCODE -ne 0) { Write-Error "Image build failed."; exit 1 }

$mount = "${ProjectDir}:/project"

if ($Shell) {
    Write-Host "==> Opening a shell in the container..." -ForegroundColor Cyan
    docker run --rm -it -v $mount $ImageName bash
    exit $LASTEXITCODE
}

Write-Host "==> Compiling the pkg..." -ForegroundColor Cyan
docker run --rm -v $mount $ImageName bash -lc "make clean; make"
$code = $LASTEXITCODE
if ($code -ne 0) { Write-Error "Build failed (exit $code)."; exit $code }

Write-Host "==> Done. Output:" -ForegroundColor Green
Get-ChildItem -Path $ProjectDir -Filter *.pkg | Select-Object Name, Length, LastWriteTime
