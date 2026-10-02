param(
    [string]$BackupRoot = "G:\Мой диск\AHexaTrader_BACKUP\RECTANGLE_ZONE_ANALYZER"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Fail([string]$Message) {
    Write-Host "[FAIL] $Message"
    exit 1
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

$bundle = Join-Path $dest "RECTANGLE_ZONE_ANALYZER_$stamp.bundle"
$zip = Join-Path $dest "RECTANGLE_ZONE_ANALYZER_$stamp.zip"
$manifest = Join-Path $dest "MANIFEST.txt"
$restore = Join-Path $dest "RESTORE_RU.txt"

Write-Host "============================================================"
Write-Host "RZA DISASTER BACKUP"
Write-Host "REPO   = $RepoRoot"
Write-Host "BACKUP = $dest"
Write-Host "============================================================"
Write-Host

# ---------------------------------------------------------------------------
# 1. Full Git history: branches, tags and commits.
# ---------------------------------------------------------------------------
Write-Host "[1/4] Creating Git bundle..."
& $Git -C $RepoRoot bundle create $bundle --all
if ($LASTEXITCODE -ne 0) {
    Fail "git bundle create failed"
}

& $Git bundle verify $bundle *> $null
if ($LASTEXITCODE -ne 0) {
    Fail "git bundle verify failed"
}
Write-Host "[PASS] Git bundle"

# ---------------------------------------------------------------------------
# 2. Snapshot of the current working tree.
#    Includes tracked + untracked non-ignored files, therefore it also saves
#    useful local edits that have not yet been committed.
# ---------------------------------------------------------------------------
Write-Host "[2/4] Creating current worktree ZIP..."

$tempRoot = Join-Path $env:TEMP ("RZA_BACKUP_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

try {
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

        $target = Join-Path $tempRoot $normalized
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

    Compress-Archive -Path (Join-Path $tempRoot "*") -DestinationPath $zip -CompressionLevel Optimal -Force
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if (-not (Test-Path -LiteralPath $zip)) {
    Fail "ZIP was not created"
}
Write-Host "[PASS] Worktree ZIP"

# ---------------------------------------------------------------------------
# 3. Manifest with reproducibility and integrity data.
# ---------------------------------------------------------------------------
Write-Host "[3/4] Writing manifest..."

$commit = (& $Git -C $RepoRoot rev-parse HEAD).Trim()
$branch = (& $Git -C $RepoRoot branch --show-current).Trim()
$remote = (& $Git -C $RepoRoot remote get-url origin 2>$null)
if ($LASTEXITCODE -ne 0) {
    $remote = ""
}
$statusLines = @(& $Git -C $RepoRoot status --short)
$statusText = if ($statusLines.Count -eq 0) { "CLEAN" } else { $statusLines -join [Environment]::NewLine }

$bundleHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $bundle).Hash
$zipHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash
$bundleBytes = (Get-Item -LiteralPath $bundle).Length
$zipBytes = (Get-Item -LiteralPath $zip).Length

@"
RECTANGLE_ZONE_ANALYZER DISASTER BACKUP
CREATED_LOCAL=$(Get-Date -Format "yyyy-MM-ddTHH:mm:ssK")
REPO_ROOT=$RepoRoot
BRANCH=$branch
COMMIT=$commit
REMOTE=$remote

BUNDLE_FILE=$(Split-Path -Leaf $bundle)
BUNDLE_BYTES=$bundleBytes
BUNDLE_SHA256=$bundleHash

ZIP_FILE=$(Split-Path -Leaf $zip)
ZIP_BYTES=$zipBytes
ZIP_SHA256=$zipHash

WORKTREE_STATUS:
$statusText
"@ | Set-Content -LiteralPath $manifest -Encoding UTF8

@"
ВОССТАНОВЛЕНИЕ RECTANGLE_ZONE_ANALYZER

1. Полное восстановление Git-репозитория со всей историей:

   git clone "$(Split-Path -Leaf $bundle)" RECTANGLE_ZONE_ANALYZER_RESTORED

2. ZIP содержит снимок текущего рабочего дерева на момент backup.
   Он включает tracked и untracked non-ignored файлы.

3. Проверка целостности:

   Get-FileHash -Algorithm SHA256 ".\$(Split-Path -Leaf $bundle)"
   Get-FileHash -Algorithm SHA256 ".\$(Split-Path -Leaf $zip)"

   Сравнить значения с MANIFEST.txt.

ВАЖНО:
.bundle является основной аварийной капсулой Git.
.zip сохраняет текущее состояние файлов, включая ещё не закоммиченные
non-ignored файлы.
"@ | Set-Content -LiteralPath $restore -Encoding UTF8

Write-Host "[PASS] Manifest"

# ---------------------------------------------------------------------------
# 4. Latest pointer.
# ---------------------------------------------------------------------------
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
