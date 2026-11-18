<#
.SYNOPSIS
1つのコマンドで全部：ツールを確認し、Docker Desktop が動いていなければ起動し、必要なら RE4 Zeebo
Edition のデータを変換し（re4zeebo/export_all.ps1）、それから Docker の BlocksDS で DS の ROM を
ビルドする。出力：ds/re4ds.nds

.EXAMPLE
./build.ps1 -InputPath "D:\Downloads\Resident Evil 4 - Zeebo Edition.zip"   # 初回
./build.ps1              # 2回目から（zip の場所は覚えている）
./build.ps1 -Run         # ビルドして melonDS で開く
./build.ps1 -Export      # 何も変わっていなくてもゲームデータを変換し直す
./build.ps1 clean        # ビルドの中間ファイルを消す

.NOTES
データは、一度も変換していないとき、または前回から変換ツール（re4zeebo/*）、文字（ds/source/text.h）、
タイトルの絵が変わったときに自動で変換し直す。
-NoMusic は音楽をそのままにする（その録音には約1.5分の静かな時間が要る）。
#>
param(
    [Parameter(Position = 0)] [string] $Target = '',
    [string] $InputPath = '',
    [switch] $Export,
    [switch] $NoMusic,
    [switch] $Run
)
$ErrorActionPreference = 'Stop'
$image = 'skylyrac/blocksds:slim-v1.24.0'
$root = $PSScriptRoot
$ds = Join-Path $root 'ds'
$out = Join-Path $root 're4zeebo/out'
$stamp = Join-Path $out 'export.stamp'        # 最後まで終わった変換の後に書く
$remember = Join-Path $out 'input_path.txt'   # zip があった場所

function Step([string]$s) { Write-Host "`n=== $s" -ForegroundColor Cyan }
function Fail([string]$s) { Write-Host "`n$s" -ForegroundColor Red; exit 1 }

# 1. ツール。
Step 'Checking the tools'
if ($PSVersionTable.PSVersion.Major -lt 7) { Fail 'PowerShell 7 is needed: start "pwsh" (PowerShell 7) and run ./build.ps1 there.' }
if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { Fail 'Docker Desktop is not installed (see README: WSL 2 and Docker Desktop).' }
docker info *> $null
if ($LASTEXITCODE -ne 0) {
    $exe = @("$env:ProgramFiles\Docker\Docker\Docker Desktop.exe", "$env:LOCALAPPDATA\Programs\DockerDesktop\Docker Desktop.exe") |
        Where-Object { Test-Path $_ } | Select-Object -First 1
    if (-not $exe) { Fail 'Docker is not running and Docker Desktop was not found: start Docker Desktop yourself, then run this again.' }
    Write-Host 'Starting Docker Desktop (up to 3 minutes)...'
    Start-Process $exe
    $t = 0
    do { Start-Sleep -Seconds 3; $t += 3; docker info *> $null } while ($LASTEXITCODE -ne 0 -and $t -lt 180)
    if ($LASTEXITCODE -ne 0) { Fail 'Docker Desktop did not get ready: wait until it shows "Engine running", then run this again.' }
}
Write-Host 'PowerShell 7, Docker: OK'

# リポジトリ自身のファイル：一部が欠けたコピー（アップロードの途中、ZIP の展開が不完全）は
# 「export_all.ps1 is not recognized」で止まっていた。
$need = @('re4zeebo/export_all.ps1', 're4zeebo/re4zeebo.ps1', 're4zeebo/gen_stage_foes.ps1', 're4zeebo/gen_stage_maps.ps1',
          're4zeebo/gen_shop_tune.ps1', 're4zeebo/gen_cutins.ps1', 're4zeebo/cutins_en.txt', 'ds/Makefile', 'ds/source/main.c')
$missing = $need | Where-Object { -not (Test-Path -LiteralPath (Join-Path $root $_)) }
if (-not (Get-ChildItem -LiteralPath (Join-Path $root 're4zeebo') -Filter *.cs -ErrorAction SilentlyContinue)) { $missing += 're4zeebo/*.cs' }
if ($missing) {
    Fail ("Files of the repository are missing:`n  " + ($missing -join "`n  ") +
          "`nDownload it again (git clone, or Code -> Download ZIP and extract all of it), then run ./build.ps1 in that folder.")
}
if ($root -match '[\[\]]') {
    # （PowerShell はパスの [ ] をワイルドカードとして読む：その中のスクリプトやファイルが「見つからない」/「認識されない」になる）
    Fail "The folder's path has [ or ] in it ($root): PowerShell can't find the files there. Rename the folder (for example to re4-zeebo-ds) and run ./build.ps1 again."
}

# 2. ゲームデータ（「clean」では行わない）。
if ($Target -ne 'clean') {
    $title = Join-Path $root 'assets/title/title_ds_edition_source.png'
    if (-not (Test-Path $title)) { Fail "The title picture is missing: put a PNG (1109 x 832 or larger) at assets/title/title_ds_edition_source.png." }
    $why = $null
    if ($Export) { $why = 'asked for (-Export)' }
    elseif (-not (Test-Path $stamp)) { $why = 'not converted yet' }
    else {
        $since = (Get-Item $stamp).LastWriteTime
        $changed = @(Get-ChildItem (Join-Path $root 're4zeebo') -File) + @(Get-Item (Join-Path $ds 'source/text.h'), $title) |
            Where-Object { $_.LastWriteTime -gt $since } | Select-Object -First 1
        if ($changed) { $why = "$($changed.Name) changed" }
    }
    if ($why) {
        Step "Converting the game data ($why)"
        if (-not $InputPath -and (Test-Path $remember)) { $InputPath = (Get-Content $remember -Raw).Trim() }
        $unpacked = Test-Path (Join-Path $out 'files')
        if (-not $InputPath -and -not $unpacked) {
            Fail 'Where is your game data? Run: ./build.ps1 -InputPath "<the Resident Evil 4 - Zeebo Edition zip>"'
        }
        if ($InputPath -and -not (Test-Path $InputPath)) { Fail "Not found: $InputPath" }
        if (-not $InputPath) { $InputPath = Join-Path $out 'files' }   # （もう展開済み：読み直さない）
        New-Item -ItemType Directory -Force $out | Out-Null
        Set-Content $remember $InputPath
        if (-not $NoMusic) { Write-Host 'The music is recorded from the PC''s sound output for about 1.5 minutes: keep other sounds off.' -ForegroundColor Yellow }
        $exportArgs = @{ InputPath = $InputPath }
        if ($NoMusic) { $exportArgs.NoMusic = $true }
        # （エスケープ：フォルダーのパスの [ ] は「&」にとってワイルドカードで、そのスクリプトが見つからなくなる）
        try { & ([Management.Automation.WildcardPattern]::Escape((Join-Path $root 're4zeebo/export_all.ps1'))) @exportArgs }
        catch { Write-Host $_ -ForegroundColor Red; Fail 'The conversion stopped: the line starting with "== " just before the error tells which step.' }
        Set-Location $root
        Set-Content $stamp (Get-Date -Format o)
    }
    else { Write-Host 'Game data: already converted, nothing changed' }
}

# 3. ROM。
Step $(if ($Target) { "make $Target" } else { 'Building the ROM (the first time also downloads BlocksDS, about 1 GB)' })
$makeArgs = @(if ($Target) { $Target })
docker run --rm -v "${ds}:/work" -w /work $image make @makeArgs
if ($LASTEXITCODE -ne 0) { Fail "The build failed ($LASTEXITCODE)." }
if ($Target -eq 'clean') { exit 0 }
Write-Host "`nDone: $(Join-Path $ds 're4ds.nds')" -ForegroundColor Green

if ($Run) {
    $melon = Join-Path $root 'tools\melonDS\melonDS.exe'
    if (Test-Path $melon) { Start-Process $melon -ArgumentList "`"$(Join-Path $ds 're4ds.nds')`"" }
    else { Write-Host 'melonDS is not at tools/melonDS/melonDS.exe: open ds/re4ds.nds yourself.' }
}
