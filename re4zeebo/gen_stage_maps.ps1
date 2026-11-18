param([string]$Root)
function Fix12([double]$x, [double]$z) { @(($x + 6.64 + 0.00374 * $z), ($z + 4.9)) }
$out = New-Object System.Collections.Generic.List[string]
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
    $rows += "    { map$st, $($tris.Count) },"
}
$out.Add('')
$out.Add('const StageMap stage_maps[12] = {')
foreach ($r in $rows) { $out.Add($r) }
$out.Add('};')
Set-Content "$Root\ds\source\stage_maps.c" $out -Encoding utf8
