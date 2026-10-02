param(
    [string]$BackupRoot = ""
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Fail([string]$Message) {
    Write-Host "[FAIL] $Message"
    exit 1
}

function Default-BackupRoot {
    # Build "My Drive" in Russian from Unicode code points.
    # The script source stays ASCII-only so Windows PowerShell 5.1 cannot
    # corrupt the path when reading a UTF-8 file without BOM.
    $codes = 0x041C,0x043E,0x0439,0x0020,0x0434,0x0438,0x0441,0x043A
    $myDrive = -join ($codes | ForEach-Object { [char]$_ })
    return Join-Path (Join-Path (Join-Path "G:\" $myDrive) "AHexaTrader_BACKUP") "RECTANGLE_ZONE_ANALYZER"
}

if ([string]::IsNullOrWhiteSpace($BackupRoot)) {
    $BackupRoot = Default-BackupRoot
}

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$Git = "D:\Git\cmd\git.exe"
if (-not (Test-Path -LiteralPath $Git)) {
    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if ($null -eq $gitCmd) {
        Fail "git.exe not found"
    }
    $Git = $gitCmd.Source
}

if (-not (Test-Path -LiteralPath $BackupRoot)) {
    New-Item -ItemType Directory -Path $BackupRoot -Force | Out-Null
}

$stamp = Get-Date -Format "yyyy-MM-dd_HHmmss"
$dest = Join-Path $BackupRoot $stamp
New-Item -ItemType Directory -Path $dest -Force | Out-Null

$bundleName = "RECTANGLE_ZONE_ANALYZER_$stamp.bundle"
$zipName = "RECTANGLE_ZONE_ANALYZER_$stamp.zip"
$bundle = Join-Path $dest $bundleName
$zip = Join-Path $dest $zipName
$manifest = Join-Path $dest "MANIFEST.txt"
$restore = Join-Path $dest "RESTORE.txt"

$stage = Join-Path $env:TEMP ("RZA_BACKUP_STAGE_" + [guid]::NewGuid().ToString("N"))
$worktreeStage = Join-Path $stage "worktree"
New-Item -ItemType Directory -Path $worktreeStage -Force | Out-Null

$stageBundle = Join-Path $stage $bundleName
$stageZip = Join-Path $stage $zipName

Write-Host "============================================================"
Write-Host "RZA DISASTER BACKUP"
Write-Host "REPO   = $RepoRoot"
Write-Host "BACKUP = $dest"
Write-Host "============================================================"
Write-Host

try {
    # -----------------------------------------------------------------------
    # 1. Create and verify the full Git bundle LOCALLY.
    #    Git never writes lock files directly into the Google Drive mount.
    # -----------------------------------------------------------------------
    Write-Host "[1/4] Creating Git bundle locally..."
    & $Git -C $RepoRoot bundle create $stageBundle --all
    if ($LASTEXITCODE -ne 0) {
        Fail "git bundle create failed"
    }

    & $Git bundle verify $stageBundle *> $null
    if ($LASTEXITCODE -ne 0) {
        Fail "git bundle verify failed"
    }
    Write-Host "[PASS] Git bundle"

    # -----------------------------------------------------------------------
    # 2. Snapshot tracked + untracked non-ignored files locally.
    # -----------------------------------------------------------------------
    Write-Host "[2/4] Creating current worktree ZIP locally..."

    $files = @(
        & $Git -C $RepoRoot -c core.quotePath=false ls-files --cached --others --exclude-standard
    )
    if ($LASTEXITCODE -ne 0) {
        Fail "git ls-files failed"
    }

    $copied = 0
    foreach ($relative in $files) {
        if ([string]::IsNullOrWhiteSpace($relative)) {
            continue
        }

        $normalized = $relative -replace '/', [IO.Path]::DirectorySeparatorChar
        $source = Join-Path $RepoRoot $normalized

        if (-not (Test-Path -LiteralPath $source -PathType Leaf)) {
            continue
        }

        $target = Join-Path $worktreeStage $normalized
        $targetDir = Split-Path -Parent $target
        if (-not (Test-Path -LiteralPath $targetDir)) {
            New-Item -ItemType Directory -Path $targetDir -Force | Out-Null
        }

        Copy-Item -LiteralPath $source -Destination $target -Force
        $copied++
    }

    if ($copied -eq 0) {
        Fail "worktree snapshot contains zero files"
    }

    Compress-Archive -Path (Join-Path $worktreeStage "*") -DestinationPath $stageZip -CompressionLevel Optimal -Force

    if (-not (Test-Path -LiteralPath $stageZip)) {
        Fail "ZIP was not created"
    }
    Write-Host "[PASS] Worktree ZIP"

    # -----------------------------------------------------------------------
    # 3. Compute integrity data, then copy finished artifacts to Google Drive.
    # -----------------------------------------------------------------------
    Write-Host "[3/4] Copying verified artifacts to Google Drive..."

    $commit = (& $Git -C $RepoRoot rev-parse HEAD).Trim()
    $branch = (& $Git -C $RepoRoot branch --show-current).Trim()
    $remote = (& $Git -C $RepoRoot remote get-url origin 2>$null)
    if ($LASTEXITCODE -ne 0) {
        $remote = ""
    }

    $statusLines = @(& $Git -C $RepoRoot status --short)
    $statusText = if ($statusLines.Count -eq 0) {
        "CLEAN"
    } else {
        $statusLines -join [Environment]::NewLine
    }

    $bundleHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $stageBundle).Hash
    $zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $stageZip).Hash
    $bundleBytes = (Get-Item -LiteralPath $stageBundle).Length
    $zipBytes = (Get-Item -LiteralPath $stageZip).Length

    Copy-Item -LiteralPath $stageBundle -Destination $bundle -Force
    Copy-Item -LiteralPath $stageZip -Destination $zip -Force

    if (-not (Test-Path -LiteralPath $bundle -PathType Leaf)) {
        Fail "bundle copy to Google Drive failed"
    }
    if (-not (Test-Path -LiteralPath $zip -PathType Leaf)) {
        Fail "ZIP copy to Google Drive failed"
    }
    if ((Get-Item -LiteralPath $bundle).Length -ne $bundleBytes) {
        Fail "bundle size mismatch after copy"
    }
    if ((Get-Item -LiteralPath $zip).Length -ne $zipBytes) {
        Fail "ZIP size mismatch after copy"
    }

    @"
RECTANGLE_ZONE_ANALYZER DISASTER BACKUP
CREATED_LOCAL=$(Get-Date -Format "yyyy-MM-ddTHH:mm:ssK")
REPO_ROOT=$RepoRoot
BRANCH=$branch
COMMIT=$commit
REMOTE=$remote

BUNDLE_FILE=$bundleName
BUNDLE_BYTES=$bundleBytes
BUNDLE_SHA256=$bundleHash

ZIP_FILE=$zipName
ZIP_BYTES=$zipBytes
ZIP_SHA256=$zipHash

WORKTREE_STATUS:
$statusText
"@ | Set-Content -LiteralPath $manifest -Encoding UTF8

    @"
RECTANGLE_ZONE_ANALYZER RESTORE GUIDE

1. Full Git restore with complete history:

   git clone "$bundleName" RECTANGLE_ZONE_ANALYZER_RESTORED

2. The ZIP contains the current worktree snapshot:
   tracked files plus untracked non-ignored files.

3. Integrity check:

   Get-FileHash -Algorithm SHA256 ".\$bundleName"
   Get-FileHash -Algorithm SHA256 ".\$zipName"

   Compare both hashes with MANIFEST.txt.

The .bundle file is the primary Git disaster-recovery capsule.
The .zip file preserves the current file state, including useful
uncommitted non-ignored files.
"@ | Set-Content -LiteralPath $restore -Encoding UTF8

    Write-Host "[PASS] Google Drive copy + manifest"

    # -----------------------------------------------------------------------
    # 4. Update latest pointer.
    # -----------------------------------------------------------------------
    Write-Host "[4/4] Updating LATEST.txt..."
    @"
LATEST_BACKUP=$stamp
PATH=$dest
COMMIT=$commit
"@ | Set-Content -LiteralPath (Join-Path $BackupRoot "LATEST.txt") -Encoding UTF8

    Write-Host "[PASS] Latest pointer"
    Write-Host
    Write-Host "============================================================"
    Write-Host "BACKUP PASS"
    Write-Host "BUNDLE_SHA256=$bundleHash"
    Write-Host "ZIP_SHA256=$zipHash"
    Write-Host "DEST=$dest"
    Write-Host "============================================================"
}
finally {
    if (Test-Path -LiteralPath $stage) {
        Remove-Item -LiteralPath $stage -Recurse -Force -ErrorAction SilentlyContinue
    }
}
