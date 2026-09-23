<#
.SYNOPSIS
Runs the deterministic crafting-category correctness and performance harness.

.EXAMPLE
.\tools\run_crafting_harness.ps1

.EXAMPLE
.\tools\run_crafting_harness.ps1 -OutputPath .\crafting-harness-report.json
#>

[CmdletBinding()]
param(
    [string]$TestBinary = (Join-Path $PSScriptRoot '..\tests\cata_test.exe'),
    [string]$OutputPath
)

$ErrorActionPreference = 'Stop'
$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$testBinaryPath = [System.IO.Path]::GetFullPath($TestBinary)
if (-not (Test-Path -LiteralPath $testBinaryPath -PathType Leaf)) {
    throw "Test binary not found: $testBinaryPath"
}

$ucrtBin = 'C:\msys64\ucrt64\bin'
if (-not (Test-Path -LiteralPath $ucrtBin -PathType Container)) {
    throw "MSYS2 UCRT64 runtime not found: $ucrtBin"
}

$freshProcessRuns = 3
$prefix = 'CRAFTING_HARNESS_JSON='

function Invoke-CraftingHarnessRun {
    param(
        [Parameter(Mandatory)]
        [int]$RunNumber
    )

    Write-Host "=== Fresh process run $RunNumber/$freshProcessRuns ==="
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $testBinaryPath
    $startInfo.ArgumentList.Add('[.][crafting_category_pipeline][benchmark]')
    $startInfo.UseShellExecute = $false
    $startInfo.RedirectStandardOutput = $true
    $startInfo.RedirectStandardError = $true
    $startInfo.Environment['PATH'] = "$ucrtBin;$($env:PATH)"

    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = $startInfo
    if (-not $process.Start()) {
        $process.Dispose()
        throw "Failed to start crafting harness run $RunNumber`: $testBinaryPath"
    }
    $stdoutTask = $process.StandardOutput.ReadToEndAsync()
    $stderrTask = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $stdout = $stdoutTask.GetAwaiter().GetResult()
    $stderr = $stderrTask.GetAwaiter().GetResult()
    $exitCode = $process.ExitCode
    $process.Dispose()
    $testOutput = @($stdout -split "`r?`n") + @($stderr -split "`r?`n")

    $testOutput | ForEach-Object { Write-Host $_ }
    if ($exitCode -ne 0) {
        throw "Crafting harness run $RunNumber failed with exit code $exitCode"
    }

    $jsonLine = $testOutput |
        ForEach-Object { [string]$_ } |
        Where-Object { $_.StartsWith($prefix, [System.StringComparison]::Ordinal) } |
        Select-Object -Last 1
    if (-not $jsonLine) {
        throw "Crafting harness run $RunNumber completed without a JSON report"
    }

    $jsonLine.Substring($prefix.Length) | ConvertFrom-Json
}

function Get-StableScenarioSignature {
    param(
        [Parameter(Mandatory)]
        [object]$Report
    )

    [ordered]@{
        recipe_source = $Report.recipe_source
        source_items = $Report.source_items
        recipes = $Report.recipes
        snapshot = $Report.snapshot
        graph = $Report.graph
        bypass = $Report.bypass
    } | ConvertTo-Json -Depth 8 -Compress
}

$reports = @()
for ($run = 1; $run -le $freshProcessRuns; ++$run) {
    $reports += Invoke-CraftingHarnessRun -RunNumber $run
}

$expectedSignature = Get-StableScenarioSignature -Report $reports[0]
for ($run = 1; $run -lt $reports.Count; ++$run) {
    $actualSignature = Get-StableScenarioSignature -Report $reports[$run]
    if ($actualSignature -cne $expectedSignature) {
        throw "Crafting harness semantic counts changed between fresh-process runs 1 and $($run + 1)"
    }
}

$medianTimings = [ordered]@{}
foreach ($timingName in $reports[0].timings_ms.psobject.Properties.Name) {
    $values = @($reports | ForEach-Object { [double]$_.timings_ms.$timingName } | Sort-Object)
    $middle = [int][Math]::Floor($values.Count / 2)
    if ($values.Count % 2 -eq 0) {
        $medianTimings[$timingName] = ($values[$middle - 1] + $values[$middle]) / 2
    } else {
        $medianTimings[$timingName] = $values[$middle]
    }
}

$gitHead = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the harness Git revision'
}
$trackedChanges = @(& git -C $repoRoot status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the harness Git status'
}
$report = [pscustomobject][ordered]@{
    schema_version = 2
    scenario = 'open_crafting_select_armor_close_fresh_process'
    fresh_process_runs = $freshProcessRuns
    stable_semantic_counts = $true
    recipe_source = $reports[0].recipe_source
    source_items = $reports[0].source_items
    recipes = $reports[0].recipes
    timings_ms_median = [pscustomobject]$medianTimings
    runs = $reports
    git = [pscustomobject]@{
        head = $gitHead
        tracked_changes = $trackedChanges.Count -gt 0
    }
}
$json = $report | ConvertTo-Json -Depth 12
Write-Host "CRAFTING_HARNESS_AGGREGATE_JSON=$($report | ConvertTo-Json -Depth 12 -Compress)"
if ($OutputPath) {
    $outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
    $outputDirectory = Split-Path -Parent $outputFullPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    }
    Set-Content -LiteralPath $outputFullPath -Value $json -Encoding utf8
}

$report
