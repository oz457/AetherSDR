<#
.SYNOPSIS
    Download ONNX Runtime prebuilt binaries for Windows x64.

.DESCRIPTION
    Downloads the official ONNX Runtime 1.18.x prebuilt release from GitHub
    and places headers / import library / DLL in third_party/onnxruntime/
    ready for CMake (find_library / find_path).

    Required only for the S_History_v2-AI CNN signal classifier.
    AetherSDR compiles and runs without it — ONNX is an optional enhancement.

.EXAMPLE
    .\setup-onnxruntime.ps1
#>

$ErrorActionPreference = "Stop"
. "$PSScriptRoot\_verify_sha256.ps1"

# Pinned to the ONNX Runtime that sherpa-onnx bundles so a build with both shares
# one runtime (no symbol-version clash). Keep in sync with setup-onnxruntime.sh.
$OrtVersion = "1.27.0"
$OrtUrl     = "https://github.com/microsoft/onnxruntime/releases/download/v${OrtVersion}/onnxruntime-win-x64-${OrtVersion}.zip"
# SHA256 of the release zip. Bump alongside the version.
$OrtSha256  = "c5c81710938e68079ff1a192b04897faabe4b43830d48f39f27ecd4e16138bfc"
$OutDir     = "third_party\onnxruntime"
$ZipFile    = "third_party\onnxruntime-win-x64-${OrtVersion}.zip"

# ── Check if already set up ──────────────────────────────────────────────
if (Test-Path "$OutDir\lib\onnxruntime.lib") {
    Write-Host "ONNX Runtime already set up in $OutDir" -ForegroundColor Green
    exit 0
}

# ── Create directories ───────────────────────────────────────────────────
New-Item -ItemType Directory -Force -Path "third_party" | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\include" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\lib" | Out-Null
New-Item -ItemType Directory -Force -Path "$OutDir\bin" | Out-Null

# ── Download ─────────────────────────────────────────────────────────────
# TEST BUILD ONLY: github.com/microsoft/onnxruntime answers 404 (2026-11-18),
# so take the same 1.27.0 binaries from the official NuGet package instead.
$NugetUrl    = "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${OrtVersion}/microsoft.ml.onnxruntime.${OrtVersion}.nupkg"
$NugetSha256 = "c725e8cefdfe4de08befb9f79b3f218c162439cdcdb97cbbb5192888d58ea669"
if (-not (Test-Path $ZipFile)) {
    Write-Host "Downloading ONNX Runtime ${OrtVersion} NuGet package..." -ForegroundColor Cyan
    Invoke-WebRequest -Uri $NugetUrl -OutFile $ZipFile
    Confirm-Sha256 -Path $ZipFile -Expected $NugetSha256
}

# ── Extract ──────────────────────────────────────────────────────────────
Write-Host "Extracting..." -ForegroundColor Cyan
$tempDir = "third_party\onnxruntime-temp"
if (Test-Path $tempDir) { Remove-Item -Recurse -Force $tempDir }
Expand-Archive -Path $ZipFile -DestinationPath $tempDir -Force

Copy-Item "$tempDir\build\native\include\*" "$OutDir\include\" -Recurse -Force
$nat = "$tempDir\runtimes\win-x64\native"
if (-not (Test-Path "$nat\onnxruntime.lib")) {
    Write-Error "onnxruntime.lib not found in NuGet package"
    exit 1
}
Copy-Item "$nat\onnxruntime.lib" "$OutDir\lib\onnxruntime.lib"
foreach ($dll in Get-ChildItem $nat -Filter "*.dll") {
    Copy-Item $dll.FullName "$OutDir\bin\$($dll.Name)"
}

# ── Cleanup ──────────────────────────────────────────────────────────────
Remove-Item -Recurse -Force $tempDir
Remove-Item -Force $ZipFile

Write-Host "ONNX Runtime ${OrtVersion} ready in $OutDir" -ForegroundColor Green
Write-Host "  Headers: $OutDir\include\"
Write-Host "  Lib:     $OutDir\lib\onnxruntime.lib"
Write-Host "  DLLs:    $OutDir\bin\"
Write-Host ""
Write-Host "Re-run CMake to pick up ONNX Runtime:" -ForegroundColor Yellow
Write-Host "  cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo"
