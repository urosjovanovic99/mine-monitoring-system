<#
.SYNOPSIS
    Launch Renode against this project from a space-free path.

.DESCRIPTION
    Renode's monitor tokenizer splits command lines on whitespace, and @path
    arguments have no quoting form that survives it. This project lives under
    "...\Master Studije\...", so Renode fails with "Could not tokenize here"
    before it ever loads the script.

    This maps the project root to a virtual drive (default P:) with subst,
    which needs no admin rights, changes nothing on disk, and leaves the
    original folder exactly where it is. STM32CubeIDE keeps using the real
    path; only Renode sees P:.

.EXAMPLE
    .\renode\run.ps1
    .\renode\run.ps1 -Script renode\water_level_sim.resc
    .\renode\run.ps1 -Drive Q:

.NOTES
    Remove the mapping when you're done:  subst P: /D
#>
param(
    [string]$Script = 'renode\mine_test.resc',
    [string]$Drive  = 'P:'
)

$ErrorActionPreference = 'Stop'

$project = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$mapping = subst | Where-Object { $_ -like "$Drive\:*" }

if ($mapping) {
    Write-Host "Reusing existing mapping: $mapping"
}
elseif (Test-Path $Drive) {
    throw "$Drive is already a real drive. Re-run with a free letter, e.g. -Drive Q:"
}
else {
    subst $Drive $project
    Write-Host "Mapped $Drive -> $project"
}

Push-Location "$Drive\"
try {
    renode $Script
}
finally {
    Pop-Location
}
