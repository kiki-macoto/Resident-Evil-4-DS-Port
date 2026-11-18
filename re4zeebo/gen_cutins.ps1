# ds/source/cutins.c を書く：ミッションのストーリーの場面（「カットイン」）、ミッションファイルの
# スクリプトのとおり（mis_data_sNN.bin、ワード5：48バイトのコマンドの場面。ワード6：文）、せりふは
# cutins_en.txt の英語で。そしてその絵（eve_stgNN.mcts、それぞれ 256 x 128）を
# ds/nitrofs/eveNN_k.bin にコピーする。
#
#   ./re4zeebo/gen_cutins.ps1 [-Files re4zeebo/out/files]
#
# 場面はページの一覧：出す絵（または無し）、話す人、文。使うコマンド：
# 201 (2, _, k) は絵 k を出す（ほかは無し）、200 (who, _, s) は文 s の1ページ、
# 202 (n) はお金。話す人：0 ナレーター、1 レオン、2 アシュリー、3 ルイス、4 サドラー。
param([string]$Files = 're4zeebo/out/files')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$Files = (Resolve-Path (Join-Path $root $Files)).Path

# 再生する場面（ミッションの場面の番号）：開始時に0、クリア時に9、そしてイベント。
$want = @{ 1 = 0, 9, 10; 4 = 0, 9, 11, 13, 14; 5 = 0, 9, 10, 17; 8 = 0, 9, 16; 11 = 0, 9, 11; 12 = 0, 9, 10 }
foreach ($n in 2, 3, 6, 7, 9, 10) { $want[$n] = 0, 9 }
# 2-12面：ミッションのイベントが再生するとおりに全部のスクリプト（main.c の foes_events()）、
# プレイ中のメッセージ（絵の無いページ）はゲームの上に出す。
foreach ($n in 2..12) { $want[$n] = 0..99 }

$en = @{}
foreach ($l in Get-Content (Join-Path $PSScriptRoot 'cutins_en.txt') -Encoding UTF8) {
    if ($l -match '^(\d\d):(\d+)\|(.*)$') { $en["$($matches[1]):$($matches[2])"] = $matches[3] }
}

function CStr([string]$s) { '"' + ($s.Replace('\', '\\').Replace('"', '\"').Replace('/', '\n')) + '"' }

$out = New-Object Text.StringBuilder
[void]$out.AppendLine('// re4zeebo/gen_cutins.ps1 がミッションファイルと re4zeebo/cutins_en.txt から生成。編集しないこと。')
[void]$out.AppendLine('#include "cutins.h"')
[void]$out.AppendLine('')
$table = @(); $pics = @{}
foreach ($stage in ($want.Keys | Sort-Object)) {
    $id = $stage.ToString('00')
    $f = (Get-ChildItem $Files -Recurse -Filter "mis_data_s$id.bin" | Select-Object -First 1).FullName
    $b = [IO.File]::ReadAllBytes($f)
    $sc = [BitConverter]::ToInt32($b, 20)
    $n = [BitConverter]::ToUInt16($b, $sc)
    $cnt = 1..$n | ForEach-Object { [BitConverter]::ToUInt16($b, $sc + $_ * 2) }
    $p = $sc + 2 + $n * 2
    $k = 0
    foreach ($s in 0..($n - 1)) {
        $pages = @(); $pic = -1; $money = 0
        for ($c = 0; $c -lt $cnt[$s]; $c++, $k++) {
            if ($want[$stage] -notcontains $s) { continue }
            $q = $p + $k * 48
            $op = [BitConverter]::ToInt16($b, $q + 6)
            $a = 4..6 | ForEach-Object { [BitConverter]::ToInt16($b, $q + $_ * 2) }
            if ($op -eq 201) { $pic = if ($a[0] -eq 2) { $a[2] } else { -1 } }
            elseif ($op -eq 202) { $money += $a[0] }
            elseif ($op -eq 200) {
                $key = "${id}:$($a[2])"
                if (-not $en.ContainsKey($key)) { throw "no English for $key" }
                foreach ($line in $en[$key].Split('/')) { if ($line.Length -gt 31) { Write-Warning "$key long line: $line" } }
                if ($pic -ge 0) { $pics["eve${id}_$pic"] = "$id/$pic" }
                $pages += "{ $pic, $($a[0]), $(CStr $en[$key]) }"
            }
        }
        if ($want[$stage] -notcontains $s -or ($pages.Count -eq 0 -and $money -eq 0)) { continue }
        $name = "s${id}_$s"
        if ($pages.Count) {
            [void]$out.AppendLine("static const CutPage $name[] = {")
            foreach ($pg in $pages) { [void]$out.AppendLine("    $pg,") }
            [void]$out.AppendLine('};')
        }
        $table += "    { $($stage - 1), $s, $($pages.Count), $(if ($pages.Count) { $name } else { 'NULL' }), $money },"
    }
}
[void]$out.AppendLine('')
[void]$out.AppendLine('const CutScene cut_scenes[] = {')
foreach ($t in $table) { [void]$out.AppendLine($t) }
[void]$out.AppendLine('};')
[void]$out.AppendLine('const int cut_scene_count = sizeof(cut_scenes) / sizeof(cut_scenes[0]);')
[IO.File]::WriteAllText((Join-Path $root 'ds/source/cutins.c'), $out.ToString().Replace("`r`n", "`n"))
"$($table.Count) scenes, $($pics.Count) pictures"

# 絵：1回の書き出しで（それぞれ自分の 256 x 128 のテクスチャ、出している間だけ読む）。
$nitro = Join-Path $root 'ds/nitrofs'
$specs = foreach ($kv in $pics.GetEnumerator()) {
    $sid, $pk = $kv.Value.Split('/')
    $dir = if ($sid -eq '01') { '3d_stg01' } else { "3d_stg${sid}_00" }
    "$($kv.Key)=$dir/eve_stg$sid.mcts#$pk@0,0,256,128"
}
& (Join-Path $PSScriptRoot 're4zeebo.ps1') -InputPath $Files -OutDir $nitro -Ds2D $specs | Out-Null
