<#
.SYNOPSIS
  Build the DS demo with BlocksDS in Docker. Output: ds/re4ds.nds
.EXAMPLE
  ./build.ps1          # build
  ./build.ps1 clean    # clean
  ./build.ps1 -Run     # build, then open in melonDS
#>
param(
    [string] $Target = '',
    [switch] $Run
)
$ErrorActionPreference = 'Stop'
$image = 'skylyrac/blocksds:slim-v1.24.0'
$ds = Join-Path $PSScriptRoot 'ds'

$makeArgs = @(if ($Target) { $Target })
docker run --rm -v "${ds}:/work" -w /work $image make @makeArgs
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

if ($Run) {
    Start-Process (Join-Path $PSScriptRoot 'tools\melonDS\melonDS.exe') -ArgumentList "`"$(Join-Path $ds 're4ds.nds')`""
}
