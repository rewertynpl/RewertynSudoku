param(
    [int]$Threads = 16,
    [string]$CpuBackend = "auto",
    [int]$CaseTimeoutSec = 30,
    [Int64]$Seed12 = 120403,
    [Int64]$Seed20 = 200405
)

$ErrorActionPreference = "Stop"

function Build-GeneratorExe {
    param(
        [string]$RepoRoot,
        [string]$OutExe
    )
    $entry = Join-Path $RepoRoot "Sudoku Level Generator RewertynPL v0.1.cpp"
    if (-not (Test-Path $entry)) {
        throw "Missing source file: $entry"
    }
    $cmd = "g++ -std=c++20 -O3 -march=native -flto -pthread -Wno-stringop-overread `"$entry`" -o `"$OutExe`" -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -lpsapi"
    Write-Host "BUILD: $OutExe"
    Invoke-Expression $cmd
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed: $OutExe"
    }
}

function Run-GeneratorCase {
    param(
        [string]$WorkDir,
        [string]$ExePath,
        [string]$Label,
        [int]$BoxRows,
        [int]$BoxCols,
        [uint64]$Target,
        [int]$DurationSec,
        [int]$ReseedSec,
        [int]$AttemptBudgetSec,
        [int]$Threads,
        [string]$CpuBackend,
        [Int64]$Seed
    )
    $args = @(
        "--no-pause",
        "--box-rows", "$BoxRows",
        "--box-cols", "$BoxCols",
        "--difficulty", "1",
        "--required-strategy", "NakedSingle",
        "--target", "$Target",
        "--threads", "$Threads",
        "--seed", "$Seed",
        "--cpu-backend", "$CpuBackend",
        "--reseed-interval-s", "$ReseedSec",
        "--attempt-time-budget-s", "$AttemptBudgetSec",
        "--max-total-time-s", "$DurationSec",
        "--single-file-only",
        "--output-folder", "generated_sudoku_files",
        "--output-file", "vip_${Label}.txt"
    )
    Write-Host "RUN: $Label N=$($BoxRows*$BoxCols)x$($BoxRows*$BoxCols) target=$Target limit=${DurationSec}s threads=$Threads reseed=${ReseedSec}s budget=${AttemptBudgetSec}s"
    Push-Location $WorkDir
    try {
        $output = & $ExePath @args 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($exitCode -ne 0) {
        throw "Run failed ($Label), exit=$exitCode"
    }

    $accepted = 0
    $attempts = 0
    $rejected = 0
    $elapsed = 0.0
    $rate = 0.0
    foreach ($line in $output) {
        if ($line -match "^Accepted:\s+([0-9]+)$") { $accepted = [UInt64]$Matches[1]; continue }
        if ($line -match "^Attempts:\s+([0-9]+)$") { $attempts = [UInt64]$Matches[1]; continue }
        if ($line -match "^Rejected:\s+([0-9]+)$") { $rejected = [UInt64]$Matches[1]; continue }
        if ($line -match "^Time:\s+([0-9]+(?:\.[0-9]+)?)s$") { $elapsed = [double]$Matches[1]; continue }
        if ($line -match "^Rate:\s+([0-9]+(?:\.[0-9]+)?)\s+puzzles/s$") { $rate = [double]$Matches[1]; continue }
    }
    return [PSCustomObject]@{
        label = $Label
        geometry = "$($BoxRows*$BoxCols)x$($BoxRows*$BoxCols) ($BoxRows x $BoxCols)"
        target = $Target
        accepted = $accepted
        attempts = $attempts
        rejected = $rejected
        elapsed_s = $elapsed
        rate = $rate
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$sourcesDir = Split-Path -Parent $scriptDir
$repoRoot = Split-Path -Parent $sourcesDir

$reportDir = Join-Path $repoRoot "plikiTMP\porownania"
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$exePath = Join-Path $repoRoot "sudoku_vip_targets.exe"
Build-GeneratorExe -RepoRoot $repoRoot -OutExe $exePath

$r12 = Run-GeneratorCase -WorkDir $repoRoot -ExePath $exePath -Label "asym12_4x3" `
    -BoxRows 4 -BoxCols 3 -Target 999 -DurationSec 15 -ReseedSec 1 -AttemptBudgetSec 1 `
    -Threads $Threads -CpuBackend $CpuBackend -Seed $Seed12

$r20 = Run-GeneratorCase -WorkDir $repoRoot -ExePath $exePath -Label "asym20_4x5" `
    -BoxRows 4 -BoxCols 5 -Target 369 -DurationSec 20 -ReseedSec 2 -AttemptBudgetSec 2 `
    -Threads $Threads -CpuBackend $CpuBackend -Seed $Seed20

# Run level1 asymmetric suite with global 5 minute timeout.
$level1Exe = Join-Path $scriptDir "sudoku_test_level1.exe"
if (-not (Test-Path $level1Exe)) {
    throw "Missing $level1Exe. Build tests first with kompilacjaTestu.bat"
}
Write-Host "RUN: sudoku_test_level1.exe --timeout-min 5 --case-timeout-s $CaseTimeoutSec"
Push-Location $scriptDir
try {
    & $level1Exe --timeout-min 5 --case-timeout-s $CaseTimeoutSec
    if ($LASTEXITCODE -ne 0) {
        throw "sudoku_test_level1.exe failed, exit=$LASTEXITCODE"
    }
} finally {
    Pop-Location
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$txtPath = Join-Path $reportDir "vip_asym_targets_${stamp}.txt"
$csvPath = Join-Path $reportDir "vip_asym_targets_${stamp}.csv"

$rows = @($r12, $r20)
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $csvPath

$txt = @()
$txt += "VIP ASYMMETRIC TARGET RUNS"
$txt += "threads=$Threads cpu_backend=$CpuBackend"
$txt += ""
foreach ($r in $rows) {
    $txt += ("{0} geo={1} target={2} accepted={3} attempts={4} rejected={5} elapsed_s={6:F3} rate={7:F6}" -f `
        $r.label, $r.geometry, $r.target, $r.accepted, $r.attempts, $r.rejected, $r.elapsed_s, $r.rate)
}
$txt += ""
$txt += "level1_suite_report=Sources/TESTS_cpp/level1_asymmetric_report.txt"
$txt += "level1_suite_progress=Sources/TESTS_cpp/level1_asymmetric_report_progress.txt"
$txt | Set-Content -Encoding UTF8 -Path $txtPath

Write-Host ""
Write-Host "DONE"
Write-Host "TXT: $txtPath"
Write-Host "CSV: $csvPath"
