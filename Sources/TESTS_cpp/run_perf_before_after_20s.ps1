param(
    [string]$StageName = "stage_asym_pairs",
    [int]$DurationSec = 15,
    [int]$BoxRows = 3,
    [int]$BoxCols = 3,
    [int]$Difficulty = 1,
    [string]$CpuBackend = "avx2",
    [Int64]$Seed = 123456,
    [int]$MinThreads = 16,
    [int]$MaxThreads = 16
)

$ErrorActionPreference = "Stop"

function Build-Exe {
    param(
        [string]$SourceDir,
        [string]$OutExe
    )
    $entry = ""
    if (Test-Path (Join-Path $SourceDir "main.cpp")) {
        $entry = "main.cpp"
    } elseif (Test-Path (Join-Path $SourceDir "Sudoku Level Generator RewertynPL v0.1.cpp")) {
        $entry = "`"Sudoku Level Generator RewertynPL v0.1.cpp`""
    } else {
        throw "Missing entry source in $SourceDir (expected main.cpp or Sudoku Level Generator RewertynPL v0.1.cpp)"
    }
    $cmd = "g++ -std=c++20 -O3 -march=native -flto -pthread -Wno-stringop-overread $entry -o `"$OutExe`" -lcomctl32 -lcomdlg32 -lshell32 -lole32 -lgdi32 -lpsapi"
    Write-Host "BUILD: $SourceDir -> $OutExe"
    Push-Location $SourceDir
    try {
        Invoke-Expression $cmd
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed in $SourceDir"
        }
    } finally {
        Pop-Location
    }
}

function Run-PerfCase {
    param(
        [string]$WorkDir,
        [string]$ExePath,
        [string]$Label,
        [int]$Threads,
        [int]$DurationSec,
        [int]$BoxRows,
        [int]$BoxCols,
        [int]$Difficulty,
        [string]$CpuBackend,
        [Int64]$Seed,
        [string]$OutFolder
    )
    $args = @(
        "--no-pause",
        "--box-rows", "$BoxRows",
        "--box-cols", "$BoxCols",
        "--difficulty", "$Difficulty",
        "--target", "1000000000",
        "--threads", "$Threads",
        "--seed", "$Seed",
        "--cpu-backend", "$CpuBackend",
        "--single-file-only",
        "--max-total-time-s", "$DurationSec",
        "--output-folder", "$OutFolder",
        "--output-file", "${Label}_t${Threads}.txt"
    )

    Write-Host "RUN: $Label threads=$Threads duration=${DurationSec}s"
    Push-Location $WorkDir
    try {
        $output = & $ExePath @args 2>&1
        $exitCode = $LASTEXITCODE
    } finally {
        Pop-Location
    }
    if ($exitCode -ne 0) {
        throw "Run failed ($Label, threads=$Threads), exit=$exitCode"
    }

    $accepted = 0
    $attempts = 0
    $elapsed = 0.0
    $rate = 0.0
    foreach ($line in $output) {
        if ($line -match "^Accepted:\s+([0-9]+)$") { $accepted = [UInt64]$Matches[1]; continue }
        if ($line -match "^Attempts:\s+([0-9]+)$") { $attempts = [UInt64]$Matches[1]; continue }
        if ($line -match "^Time:\s+([0-9]+(?:\.[0-9]+)?)s$") { $elapsed = [double]$Matches[1]; continue }
        if ($line -match "^Rate:\s+([0-9]+(?:\.[0-9]+)?)\s+puzzles/s$") { $rate = [double]$Matches[1]; continue }
    }
    return [PSCustomObject]@{
        label = $Label
        threads = $Threads
        accepted = $accepted
        attempts = $attempts
        elapsed_s = $elapsed
        rate = $rate
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent (Split-Path -Parent $scriptDir)
$snapshotRoot = Join-Path $repoRoot "plikiTMP\stage_snapshots\$StageName\before"
if (-not (Test-Path (Join-Path $snapshotRoot "main.cpp"))) {
    throw "Missing before snapshot: $snapshotRoot"
}

if ($MaxThreads -le 0) {
    $MaxThreads = [int]$env:NUMBER_OF_PROCESSORS
}
if ($MaxThreads -lt $MinThreads) {
    throw "MaxThreads must be >= MinThreads"
}

$reportDir = Join-Path $repoRoot "plikiTMP\porownania"
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$beforeExe = Join-Path $snapshotRoot "sudoku_gen_before.exe"
$afterExe = Join-Path $repoRoot "sudoku_gen_after.exe"

Build-Exe -SourceDir $snapshotRoot -OutExe $beforeExe
Build-Exe -SourceDir $repoRoot -OutExe $afterExe

$outFolder = Join-Path $repoRoot ("plikiTMP\\testy\\perf_before_after_{0}s" -f $DurationSec)
New-Item -ItemType Directory -Force -Path $outFolder | Out-Null

$rows = @()
for ($t = $MinThreads; $t -le $MaxThreads; ++$t) {
    $before = Run-PerfCase -WorkDir $snapshotRoot -ExePath $beforeExe -Label "before" -Threads $t `
        -DurationSec $DurationSec -BoxRows $BoxRows -BoxCols $BoxCols -Difficulty $Difficulty `
        -CpuBackend $CpuBackend -Seed $Seed -OutFolder $outFolder

    $after = Run-PerfCase -WorkDir $repoRoot -ExePath $afterExe -Label "after" -Threads $t `
        -DurationSec $DurationSec -BoxRows $BoxRows -BoxCols $BoxCols -Difficulty $Difficulty `
        -CpuBackend $CpuBackend -Seed $Seed -OutFolder $outFolder

    $base = [Math]::Max(0.000001, $before.rate)
    $deltaPct = (($after.rate - $before.rate) / $base) * 100.0
    $rows += [PSCustomObject]@{
        threads = $t
        before_rate = $before.rate
        after_rate = $after.rate
        delta_pct = $deltaPct
        before_accepted = $before.accepted
        after_accepted = $after.accepted
        before_attempts = $before.attempts
        after_attempts = $after.attempts
        before_elapsed_s = $before.elapsed_s
        after_elapsed_s = $after.elapsed_s
    }
}

$stamp = Get-Date -Format "yyyyMMdd_HHmmss"
$csvPath = Join-Path $reportDir ("perf_before_after_{0}s_{1}.csv" -f $DurationSec, $stamp)
$txtPath = Join-Path $reportDir ("perf_before_after_{0}s_{1}.txt" -f $DurationSec, $stamp)
$rows | Export-Csv -NoTypeInformation -Encoding UTF8 -Path $csvPath

$avgBefore = ($rows | Measure-Object -Property before_rate -Average).Average
$avgAfter = ($rows | Measure-Object -Property after_rate -Average).Average
$avgDelta = ($rows | Measure-Object -Property delta_pct -Average).Average

$txt = @()
$txt += ("PERF BEFORE/AFTER {0}s (sequential, per-thread)" -f $DurationSec)
$txt += "stage_snapshot=$StageName"
$txt += "threads=$MinThreads..$MaxThreads"
$txt += "geometry=${BoxRows}x${BoxCols}"
$txt += "difficulty=$Difficulty"
$txt += "cpu_backend=$CpuBackend"
$txt += "duration_sec=$DurationSec"
$txt += ""
$txt += ("avg_before_rate={0:F6}" -f $avgBefore)
$txt += ("avg_after_rate={0:F6}" -f $avgAfter)
$txt += ("avg_delta_pct={0:F6}" -f $avgDelta)
$txt += ""
foreach ($r in $rows) {
    $txt += ("t={0} before={1:F6} after={2:F6} delta_pct={3:F6} before_acc={4} after_acc={5}" -f `
        $r.threads, $r.before_rate, $r.after_rate, $r.delta_pct, $r.before_accepted, $r.after_accepted)
}
$txt | Set-Content -Encoding UTF8 -Path $txtPath

Write-Host ""
Write-Host "DONE"
Write-Host "CSV: $csvPath"
Write-Host "TXT: $txtPath"
Write-Host ("AVG before={0:F3} after={1:F3} delta={2:F3}%" -f $avgBefore, $avgAfter, $avgDelta)
