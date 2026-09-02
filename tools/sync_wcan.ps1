<#
.SYNOPSIS
    Vendors the WCAN driver library and headers from the SIL repository.

.DESCRIPTION
    WCAN is a shared-memory virtual CAN bus developed in the SIL repository,
    under sil_lib/wcan. It is vendored here the same way the PEAK, InnoMaker,
    TouCAN and SYS TEC drivers are: a prebuilt library in
    hardware_integration/lib/Windows plus its public headers alongside the
    other driver headers.

    Applications that use this stack select the WCAN driver and nothing more.
    They do not build or supply the library, exactly as they do not supply
    PCANBasic.

    Two library variants are produced, because a static library must be built
    by a toolchain compatible with the one linking it:

      wcan_x64.lib    MSVC, 64-bit
      libwcan_x64.a   MinGW, 64-bit

    Whichever toolchain is missing is skipped with a warning.

.PARAMETER Source
    Path to a working copy of the SIL repository. Falls back to the SIL_REPO
    environment variable when omitted.

.PARAMETER Check
    Report whether the vendored libraries are stale without modifying anything.
    Exits 1 if the recorded commit differs from the source repository's.

.EXAMPLE
    .\tools\sync_wcan.ps1 -Source C:\src\SIL

.EXAMPLE
    .\tools\sync_wcan.ps1 -Check
#>

[CmdletBinding()]
param(
    [string]$Source,
    [switch]$Check
)

$ErrorActionPreference = "Stop"

$UpstreamSubdir = "sil_lib/wcan"
$LibDir         = "hardware_integration/lib/Windows"
$IncludeDir     = "hardware_integration/include/isobus/hardware_integration"
$ProvenanceName = "WCAN_UPSTREAM"

$Sources = @("wcan.c", "wcan_validate.c", "wcan_airtime.c")
$Headers = @(
    "wcan.h",
    "wcan_types.h",
    "wcan_validate.h",
    "wcan_airtime.h",
    "wcan_export.h",
    "wcan_layout.h"
)

$MsvcLibrary  = "wcan_x64.lib"
$MinGwLibrary = "libwcan_x64.a"

$DefaultVcVars = "C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Auxiliary/Build/vcvars64.bat"
$DefaultMinGw  = "C:/_tools/Msys2Toolchains/msys64-gcc-13.2.0/mingw64/bin"

function Fail([string]$message) {
    Write-Host "error: $message" -ForegroundColor Red
    exit 2
}

function Resolve-SourceRepo([string]$candidate) {
    if ([string]::IsNullOrWhiteSpace($candidate) -and (Test-Path env:SIL_REPO)) {
        $candidate = $env:SIL_REPO
    }
    if ([string]::IsNullOrWhiteSpace($candidate)) {
        Fail "no SIL repository given. Pass -Source <path> or set SIL_REPO."
    }
    if (-not (Test-Path -LiteralPath $candidate)) {
        Fail "source path does not exist: $candidate"
    }
    $full = (Resolve-Path -LiteralPath $candidate).Path
    if (-not (Test-Path -LiteralPath (Join-Path $full ".git"))) {
        Fail "source is not a git repository: $full"
    }
    if (-not (Test-Path -LiteralPath (Join-Path $full $UpstreamSubdir))) {
        Fail "source has no '$UpstreamSubdir' directory: $full"
    }
    return $full
}

function Get-RecordedCommit([string]$provenance) {
    if (-not (Test-Path -LiteralPath $provenance)) {
        return $null
    }
    foreach ($line in Get-Content -LiteralPath $provenance) {
        if ($line -match "^commit:\s*(\S+)") {
            return $Matches[1]
        }
    }
    return $null
}

$repoRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot "..")).Path
$libPath = Join-Path $repoRoot $LibDir
$includePath = Join-Path $repoRoot $IncludeDir
$provenancePath = Join-Path $libPath $ProvenanceName

$sourcePath = Resolve-SourceRepo $Source
$sourceWcan = Join-Path $sourcePath $UpstreamSubdir

$commit = (& git -C $sourcePath rev-parse HEAD).Trim()
$branch = (& git -C $sourcePath rev-parse --abbrev-ref HEAD).Trim()
$url = (& git -C $sourcePath remote get-url origin 2>$null)
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($url)) {
    $url = "(no origin remote)"
}
$url = $url.Trim()

Write-Host "Upstream : $url"
Write-Host "Branch   : $branch"
Write-Host "Commit   : $commit"
Write-Host ""

$recorded = Get-RecordedCommit $provenancePath

if ($Check) {
    if ($null -eq $recorded) {
        Write-Host "No vendored WCAN library found." -ForegroundColor Yellow
        exit 1
    }
    if ($recorded -ne $commit) {
        Write-Host "Vendored library was built from $recorded, upstream is at $commit." -ForegroundColor Yellow
        Write-Host "Run without -Check to rebuild it."
        exit 1
    }
    Write-Host "In sync: libraries built from $commit."
    exit 0
}

$status = & git -C $sourcePath status --porcelain -- $UpstreamSubdir
if (-not [string]::IsNullOrWhiteSpace($status)) {
    Write-Host "warning: source has uncommitted changes under $UpstreamSubdir/:" -ForegroundColor Yellow
    $status -split "`n" | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
    Write-Host "The recorded commit will not describe the built binaries." -ForegroundColor Yellow
    Write-Host ""
}

New-Item -ItemType Directory -Force -Path $libPath | Out-Null
New-Item -ItemType Directory -Force -Path $includePath | Out-Null

$objDir = Join-Path ([System.IO.Path]::GetTempPath()) "wcan_vendor_$PID"
New-Item -ItemType Directory -Force -Path $objDir | Out-Null

$built = @()

try {
    # --- MSVC -------------------------------------------------------------
    $vcvars = if (Test-Path env:WCAN_VCVARS) { $env:WCAN_VCVARS } else { $DefaultVcVars }
    if (Test-Path -LiteralPath $vcvars) {
        $objects = ($Sources | ForEach-Object { "`"$sourceWcan\$_`"" }) -join " "
        $target = Join-Path $libPath $MsvcLibrary
        $command = "call `"$vcvars`" >nul 2>&1 && cd /d `"$objDir`" && cl /nologo /std:c11 /W3 /O2 /c $objects /I`"$sourceWcan`" >nul && lib /nologo /OUT:`"$target`" *.obj >nul"
        & $env:ComSpec /c $command
        if ($LASTEXITCODE -eq 0 -and (Test-Path -LiteralPath $target)) {
            $built += "$MsvcLibrary ($((Get-Item -LiteralPath $target).Length) bytes)"
        } else {
            Write-Host "warning: MSVC build failed; $MsvcLibrary not updated." -ForegroundColor Yellow
        }
        Remove-Item -LiteralPath (Join-Path $objDir "*.obj") -Force -ErrorAction SilentlyContinue
    } else {
        Write-Host "warning: vcvars64.bat not found, skipping the MSVC library." -ForegroundColor Yellow
        Write-Host "         Set WCAN_VCVARS to override." -ForegroundColor Yellow
    }

    # --- MinGW ------------------------------------------------------------
    $mingw = if (Test-Path env:WCAN_MINGW_BIN) { $env:WCAN_MINGW_BIN } else { $DefaultMinGw }
    $gcc = Join-Path $mingw "gcc.exe"
    $ar = Join-Path $mingw "ar.exe"
    if ((Test-Path -LiteralPath $gcc) -and (Test-Path -LiteralPath $ar)) {
        $objects = @()
        $failed = $false
        foreach ($file in $Sources) {
            $object = Join-Path $objDir ([System.IO.Path]::GetFileNameWithoutExtension($file) + ".o")
            & $gcc -std=c11 -O2 -Wall -Wextra -c (Join-Path $sourceWcan $file) -I $sourceWcan -o $object
            if ($LASTEXITCODE -ne 0) {
                $failed = $true
                break
            }
            $objects += $object
        }
        $target = Join-Path $libPath $MinGwLibrary
        if (-not $failed) {
            if (Test-Path -LiteralPath $target) {
                Remove-Item -LiteralPath $target -Force
            }
            & $ar rcs $target @objects
            if ($LASTEXITCODE -eq 0) {
                $built += "$MinGwLibrary ($((Get-Item -LiteralPath $target).Length) bytes)"
            } else {
                Write-Host "warning: archiving failed; $MinGwLibrary not updated." -ForegroundColor Yellow
            }
        } else {
            Write-Host "warning: MinGW build failed; $MinGwLibrary not updated." -ForegroundColor Yellow
        }
    } else {
        Write-Host "warning: MinGW gcc not found at $mingw, skipping $MinGwLibrary." -ForegroundColor Yellow
        Write-Host "         Set WCAN_MINGW_BIN to override." -ForegroundColor Yellow
    }
}
finally {
    Remove-Item -LiteralPath $objDir -Recurse -Force -ErrorAction SilentlyContinue
}

if ($built.Count -eq 0) {
    Fail "no library could be built; nothing was vendored."
}

foreach ($header in $Headers) {
    $from = Join-Path $sourceWcan $header
    if (-not (Test-Path -LiteralPath $from)) {
        Fail "missing upstream header: $from"
    }
    Copy-Item -LiteralPath $from -Destination (Join-Path $includePath $header) -Force
}

$timestamp = (Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHH:mm:ssZ")
$provenance = @"
# Provenance of the vendored WCAN driver library.
# Written by tools/sync_wcan.ps1 - do not edit by hand.
#
# WCAN is developed in the SIL repository under sil_lib/wcan. Only the built
# libraries and the public headers are vendored here; the sources are not.
# Change WCAN upstream, then re-run the sync script.

url:       $url
branch:    $branch
commit:    $commit
subdir:    $UpstreamSubdir
libraries: $($built -join ', ')
headers:   $($Headers -join ', ')
built:     $timestamp
"@
Set-Content -LiteralPath $provenancePath -Value $provenance -Encoding ASCII

Write-Host ""
foreach ($entry in $built) {
    Write-Host "Built $entry"
}
Write-Host "Headers copied to $IncludeDir"
Write-Host "Provenance written to $LibDir/$ProvenanceName"
