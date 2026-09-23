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
    throw "Failed to start crafting harness: $testBinaryPath"
}
$stdoutTask = $process.StandardOutput.ReadToEndAsync()
$stderrTask = $process.StandardError.ReadToEndAsync()
$process.WaitForExit()
$stdout = $stdoutTask.GetAwaiter().GetResult()
$stderr = $stderrTask.GetAwaiter().GetResult()
$exitCode = $process.ExitCode
$testOutput = @($stdout -split "`r?`n") + @($stderr -split "`r?`n")

$testOutput | ForEach-Object { Write-Host $_ }
if ($exitCode -ne 0) {
    throw "Crafting harness failed with exit code $exitCode"
}

$prefix = 'CRAFTING_HARNESS_JSON='
$jsonLine = $testOutput |
    ForEach-Object { [string]$_ } |
    Where-Object { $_.StartsWith($prefix, [System.StringComparison]::Ordinal) } |
    Select-Object -Last 1
if (-not $jsonLine) {
    throw 'Crafting harness completed without a JSON report'
}

$report = $jsonLine.Substring($prefix.Length) | ConvertFrom-Json
$gitHead = (& git -C $repoRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the harness Git revision'
}
$trackedChanges = @(& git -C $repoRoot status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0) {
    throw 'Unable to read the harness Git status'
}
$report | Add-Member -NotePropertyName git -NotePropertyValue ([pscustomobject]@{
    head = $gitHead
    tracked_changes = $trackedChanges.Count -gt 0
})
$json = $report | ConvertTo-Json -Depth 8
if ($OutputPath) {
    $outputFullPath = [System.IO.Path]::GetFullPath($OutputPath)
    $outputDirectory = Split-Path -Parent $outputFullPath
    if ($outputDirectory -and -not (Test-Path -LiteralPath $outputDirectory)) {
        New-Item -ItemType Directory -Path $outputDirectory | Out-Null
    }
    Set-Content -LiteralPath $outputFullPath -Value $json -Encoding utf8
}

$report
