# ds/source/stage_maps.c を書く（pwsh ./re4zeebo/gen_stage_maps.ps1 -Root .）。ステージのマップの
# モデル（map_stg_NN.mcms、re4zeebo.ps1 の展開が re4zeebo/out/models に書く OBJ）から。それぞれ
# ワールド単位のステージの平らな平面図（y はほぼ0）、三角形はグループ draw_01.. に分かれる（床、
# 上の階の床、その上に立つもの）。下画面のマップに描く（vital.c）。
param([string]$Root)
# 12面の地点とそのモデル（gen_stage_foes.ps1 / DsExport.cs の Stage12Fix と同じ）。
function Fix12([double]$x, [double]$z) { @(($x + 6.64 + 0.00374 * $z), ($z + 4.9)) }
$out = New-Object System.Collections.Generic.List[string]
$out.Add('// re4zeebo/gen_stage_maps.ps1 がステージのマップのモデル（map_stg_NN.mcms）から生成。編集しないこと。')
$out.Add('#include "stage_maps.h"')
$out.Add('')
$rows = @()
foreach ($n in 1..12) {
    $st = '{0:d2}' -f $n
    $obj = Get-ChildItem -Recurse "$Root\re4zeebo\out\models" -Filter "map_stg_$st.obj" | Select-Object -First 1
    $v = @(); $grp = -1; $tris = @()
    foreach ($l in Get-Content $obj.FullName) {
        if ($l -match '^v ') {
            $a = $l.Split(' ', [StringSplitOptions]::RemoveEmptyEntries); $x = [double]$a[1]; $z = [double]$a[3]
            if ($n -eq 12) { $x -= 468; $z -= 988; $x, $z = Fix12 $x $z }
            $v += ,@($x, $z)
        }
        elseif ($l -match '^g ') { $grp++ }
        elseif ($l -match '^f ') {
            $ids = $l.Substring(2).Split(' ', [StringSplitOptions]::RemoveEmptyEntries) | ForEach-Object { [int]($_.Split('/')[0]) - 1 }
            for ($k = 1; $k + 1 -lt $ids.Count; $k++) { $tris += ,@([Math]::Max($grp, 0), $ids[0], $ids[$k], $ids[$k + 1]) }
        }
    }
    $out.Add("static const MapTri map$st[] = {")
    foreach ($t in $tris) {
        $p = $t[1..3] | ForEach-Object { '{0}, {1}' -f [Math]::Round($v[$_][0]), [Math]::Round($v[$_][1]) }
        $out.Add(('    {{ {0}, {{ {1} }} }},' -f $t[0], ($p -join ', ')))
    }
    $out.Add('};')
    $rows += "    { map$st, $($tris.Count) },   // $n"
}
$out.Add('')
$out.Add('const StageMap stage_maps[12] = {')
foreach ($r in $rows) { $out.Add($r) }
$out.Add('};')
Set-Content "$Root\ds\source\stage_maps.c" $out -Encoding utf8
