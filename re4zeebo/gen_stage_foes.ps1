# Writes ds/source/stage_foes.c (pwsh ./re4zeebo/gen_stage_foes.ps1 -Root .) from the mission files (enemy records, section at header +8;
# objects, header +12) and the stage data's spawn points (stg_data section 3).
param([string]$Root)
$files = "$Root\re4zeebo\out\files"
function I32($b, $o) { [BitConverter]::ToInt32($b, $o) }
function S8($v) { if ($v -ge 128) { $v - 256 } else { $v } }
# Stage 12's points against its model (as DsExport.cs's Stage12Fix moves its collision): a few
# units more than its (-468, -988), a little less in x the further along z.
function Fix12([double]$x, [double]$z) { @(($x + 6.64 + 0.00374 * $z), ($z + 4.9)) }
$out = New-Object System.Collections.Generic.List[string]
$out.Add('// Generated from the game''s mission files (mis_data_sNN: the enemy records and the objects)')
$out.Add('// and stage data (stg_data_NN section 3: the spawn points the records name). See stages.h.')
$out.Add('#include "stages.h"')
$out.Add('')
$rows = @()
foreach ($n in 1..12) {
    $st = '{0:d2}' -f $n
    $ox = 0; $oz = 0
    if ($n -eq 12) { $ox = -468; $oz = -988 }   # stage 12's data sits off its model
    $sd = [IO.File]::ReadAllBytes((Get-ChildItem -Recurse $files -Filter "stg_data_$st.bin" | Select-Object -First 1).FullName)
    $s3 = I32 $sd 16; $pc = I32 $sd $s3
    $pts = @(); for ($i = 0; $i -lt $pc; $i++) {
        $o = $s3 + 4 + $i * 12; $x = (I32 $sd $o) / 256 + $ox; $z = (I32 $sd ($o + 8)) / 256 + $oz
        if ($n -eq 12) { $x, $z = Fix12 $x $z }
        $pts += ,@([Math]::Round($x), [Math]::Round((I32 $sd ($o + 4)) / 256), [Math]::Round($z)) }
    $md = [IO.File]::ReadAllBytes("$files\3d_base\mis_data_s$st.bin")
    $s1 = I32 $md 8; $rc = I32 $md $s1
    $foes = @(); $start = $null; $ally = $null
    for ($i = 0; $i -lt $rc; $i++) {
        $o = $s1 + 4 + $i * 10; $r = $md[$o..($o + 9)]
        $type = S8 $r[2]
        if ($r[1] -eq 2) { continue }                     # (the harder setting's own enemies)
        $p = $pts[$r[4]]
        if ($type -eq 0) { $start = @($p[0], $p[1], $p[2], $r[5]) }
        elseif ($type -eq 1) { $ally = $p }
        elseif ($type -ge 2) { $foes += ,@($r[0], $type, $p[0], $p[1], $p[2], $r[5], (S8 $r[8])) }
    }
    $s2 = I32 $md 12; $ec = I32 $md $s2
    $objs = @()
    for ($i = 0; $i -lt $ec; $i++) {
        $o = $s2 + 4 + $i * 16; $k = $md[$o]
        if ($k -notin 1, 2, 3, 4, 5, 9) { continue }
        $x = (I32 $md ($o + 4)) / 256 + $ox; $z = (I32 $md ($o + 12)) / 256 + $oz
        if ($n -eq 12) { $x, $z = Fix12 $x $z }
        $objs += ,@($k, [Math]::Round($x), [Math]::Round((I32 $md ($o + 8)) / 256), [Math]::Round($z), (S8 $md[$o + 2]), $md[$o + 1])
    }
    $out.Add("static const StageFoe foes$st[] = {")
    foreach ($f in $foes) { $out.Add(('    {{ {0}, {1}, {{ {2}, {3}, {4} }}, {5}, {6} }},' -f $f[0], $f[1], $f[2], $f[3], $f[4], $f[5], $f[6])) }
    $out.Add('};')
    $out.Add("static const StageThing things$st[] = {")
    foreach ($t in $objs) { $out.Add(('    {{ {0}, {{ {1}, {2}, {3} }}, {4}, {5} }},' -f $t[0], $t[1], $t[2], $t[3], $t[4], $t[5])) }
    if ($objs.Count -eq 0) { $out.Add('    { 0, { 0, 0, 0 }, -1, 0 },') }
    $out.Add('};')
    $al = if ($ally) { '1, {{ {0}, {1}, {2} }}' -f $ally[0], $ally[1], $ally[2] } else { '0, { 0, 0, 0 }' }
    $rows += ('    {{ {0}, foes{1}, {{ {2}, {3}, {4} }}, {5}, {6}, {7}, things{1} }},   // {8}' -f $foes.Count, $st, $start[0], $start[1], $start[2], $start[3], $al, $objs.Count, $n)
}
$out.Add('')
$out.Add('const StageFoes stage_foes[STAGE_COUNT] = {')
foreach ($r in $rows) { $out.Add($r) }
$out.Add('};')
Set-Content "$Root\ds\source\stage_foes.c" $out -Encoding utf8

