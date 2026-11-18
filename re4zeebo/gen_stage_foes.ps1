# ds/source/stage_foes.c を書く（pwsh ./re4zeebo/gen_stage_foes.ps1 -Root .）。ミッションファイル（敵のレコード、ヘッダー +8 のセクション。
# 物、ヘッダー +12）とステージのデータの出現地点（stg_data のセクション3）から。
param([string]$Root)
$files = "$Root\re4zeebo\out\files"
function I32($b, $o) { [BitConverter]::ToInt32($b, $o) }
function S8($v) { if ($v -ge 128) { $v - 256 } else { $v } }
# 12面の地点とそのモデル（DsExport.cs の Stage12Fix が当たり判定を動かすのと同じ）：(-468, -988) より
# 数単位多く、z の先ほど x を少し小さく。
function Fix12([double]$x, [double]$z) { @(($x + 6.64 + 0.00374 * $z), ($z + 4.9)) }
$out = New-Object System.Collections.Generic.List[string]
$out.Add('// 原作のミッションファイル（mis_data_sNN：敵のレコードと物）と')
$out.Add('// ステージのデータ（stg_data_NN のセクション3：レコードが指す出現地点）から生成。stages.h を参照。')
$out.Add('#include "stages.h"')
$out.Add('')
$rows = @(); $treasureRows = @()
foreach ($n in 1..12) {
    $st = '{0:d2}' -f $n
    $ox = 0; $oz = 0
    if ($n -eq 12) { $ox = -468; $oz = -988 }   # 12面のデータはモデルからずれている
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
        if ($r[1] -eq 2) { continue }                     # （難しい設定専用の敵）
        $p = $pts[$r[4]]
        if ($type -eq 0) { $start = @($p[0], $p[1], $p[2], $r[5]) }
        elseif ($type -eq 1) { $ally = $p }
        elseif ($type -ge 2) { $foes += ,@($r[0], $type, $p[0], $p[1], $p[2], $r[5], (S8 $r[8]), $r[3], $i, $r[7], $r[6]) }
    }
    $s2 = I32 $md 12; $ec = I32 $md $s2
    $objs = @(); $thingOf = @{}
    for ($i = 0; $i -lt $ec; $i++) {
        $o = $s2 + 4 + $i * 16; $k = $md[$o]
        if ($k -notin 1, 2, 3, 4, 5, 9) { continue }
        $x = (I32 $md ($o + 4)) / 256 + $ox; $z = (I32 $md ($o + 12)) / 256 + $oz
        if ($n -eq 12) { $x, $z = Fix12 $x $z }
        $thingOf[$i] = $objs.Count
        $objs += ,@($k, [Math]::Round($x), [Math]::Round((I32 $md ($o + 8)) / 256), [Math]::Round($z), (S8 $md[$o + 2]), $md[$o + 1])
    }
    # ミッションのイベント（ヘッダー +16 のセクション：u32 n、n x 28バイト。libbio4af.so の
    # GM_Data_MisParam の EveTbl）：+4 カテゴリ（0 開始、1 終わり、2 条件だけで、3 敵のレコード trg が
    # 持つ、4 物 trg が持つ、5 イベント地点 trg で）、+5 trg、+7 条件（GM_CtrlMain_Event::Init の表の順：
    # 0 なし、1 時間、2 倒した数、3 レコードを倒した、4 イベントを再生した、5 ミッションフラグ、6 持ち主が
    # 壊された、7 アイテムを持っている、8 イベントの壁に触れた）、+8 その値、+12 / +14 再生する
    # スクリプト（s16、-1 なし）、+18 イベント地点の届く範囲。スクリプト：ヘッダー +20、アニメの
    # スクリプトと同じ（u16 n、n 個の u16 の数、48バイトのコマンド：+6 に ID、+8 から値）。ここで残すもの：
    # 203 アイテム入手、204 アイテム譲渡、205 ミッションのタイマー（Set_LimitTime）、206 ミッションフラグ、
    # 208 イベント削除、210 敵のグループの入場（GM_Ctrl_ObjScriptEv のコマンド、200 から、その表の順）。
    $sc = I32 $md 20; $ns = [BitConverter]::ToUInt16($md, $sc); $scp = @{}
    $q = $sc + 2 + 2 * $ns
    for ($i = 0; $i -lt $ns; $i++) {
        $cmds = @()
        for ($j = 0; $j -lt [BitConverter]::ToUInt16($md, $sc + 2 + 2 * $i); $j++) {
            $id = [BitConverter]::ToUInt16($md, $q + 6)
            if ($id -in 203, 204, 205, 206, 208, 210) { $cmds += ,@($id, (I32 $md ($q + 8)), (I32 $md ($q + 12))) }
            $q += 48 }
        $scp[$i] = $cmds }
    $se = I32 $md 16; $ne = I32 $md $se
    $evs = @(); $ops = @()
    for ($i = 0; $i -lt $ne; $i++) {
        $p = $se + 4 + $i * 28
        $ctg = $md[$p + 4]; $trg = S8 $md[$p + 5]
        if ($ctg -eq 4) { $trg = if ($thingOf.ContainsKey([int]$trg)) { $thingOf[[int]$trg] } else { -1 } }
        $first = $ops.Count
        foreach ($k in 12, 14) { $s = [BitConverter]::ToInt16($md, $p + $k); if ($s -ge 0) { foreach ($c in $scp[[int]$s]) { $ops += ,$c } } }
        $evs += ,@($ctg, $trg, $md[$p + 7], $md[$p + 8], [BitConverter]::ToInt16($md, $p + 18), $first, ($ops.Count - $first), $md[$p + 20], $md[$p + 21], [BitConverter]::ToInt16($md, $p + 12), [BitConverter]::ToInt16($md, $p + 14))
    }
    # イベント地点（stg_data のセクション4、GM_Data_StgParam の Ev_Pos：u32 n、n x s32 x, y, z）と
    # イベントの壁（kind 2 の当たり判定のグループ、その順：GM_Util_BoundStage::Check_EvHit がプレイヤーが
    # 触れたことを示す）、それぞれその三角形を横切る一番長い水平な線として。
    $s4 = I32 $sd 20; $epc = I32 $sd $s4
    $epts = @(); for ($i = 0; $i -lt $epc; $i++) {
        $o = $s4 + 4 + $i * 12; $x = (I32 $sd $o) / 256 + $ox; $z = (I32 $sd ($o + 8)) / 256 + $oz
        if ($n -eq 12) { $x, $z = Fix12 $x $z }
        $epts += ,@([Math]::Round($x), [Math]::Round((I32 $sd ($o + 4)) / 256), [Math]::Round($z)) }
    $s0 = I32 $sd 8; $gn = I32 $sd $s0; $tf = $s0 + 12 + $gn * 16
    $ewalls = @()
    for ($g = 0; $g -lt $gn; $g++) {
        $gp = $s0 + 12 + $g * 16
        if ($sd[$gp + 4] -ne 2) { continue }
        $vs = @(); $y0 = 1e9; $y1 = -1e9
        for ($t = (I32 $sd ($gp + 8)); $t -lt (I32 $sd ($gp + 8)) + (I32 $sd ($gp + 12)); $t++) {
            for ($k = 0; $k -lt 3; $k++) {
                $x = (I32 $sd ($tf + $t * 36 + $k * 12)) / 256 + $ox; $y = (I32 $sd ($tf + $t * 36 + $k * 12 + 4)) / 256; $z = (I32 $sd ($tf + $t * 36 + $k * 12 + 8)) / 256 + $oz
                if ($n -eq 12) { $x, $z = Fix12 $x $z }
                $vs += ,@($x, $z); $y0 = [Math]::Min($y0, $y); $y1 = [Math]::Max($y1, $y) } }
        $best = 0; $a = $vs[0]; $b2 = $vs[0]
        foreach ($u in $vs) { foreach ($v in $vs) { $d = ($u[0] - $v[0]) * ($u[0] - $v[0]) + ($u[1] - $v[1]) * ($u[1] - $v[1]); if ($d -gt $best) { $best = $d; $a = $u; $b2 = $v } } }
        $ewalls += ,@([Math]::Round($a[0]), [Math]::Round($a[1]), [Math]::Round($b2[0]), [Math]::Round($b2[1]), [Math]::Round($y0), [Math]::Round($y1))
    }
    # 宝（kind 7 の物、その順）：それぞれがくれるのは、それに付いたイベント（カテゴリ4、その物）の
    # アイテム、そのスクリプトが最初にくれるアイテム。
    $tre = @(); for ($i = 0; $i -lt $ec; $i++) { if ($md[$s2 + 4 + $i * 16] -eq 7) { $tre += $i } }
    $titems = @()
    foreach ($ti in $tre) {
        $item = -1
        for ($e = 0; $e -lt $ne -and $item -lt 0; $e++) {
            $pe = $se + 4 + $e * 28
            if ($md[$pe + 4] -ne 4 -or $md[$pe + 5] -ne $ti) { continue }
            foreach ($k in 12, 14) { $s = [BitConverter]::ToInt16($md, $pe + $k); if ($s -ge 0 -and $item -lt 0) { foreach ($c in $scp[[int]$s]) { if ($c[0] -eq 203 -and $item -lt 0) { $item = $c[1] } } } }
        }
        $titems += $item }
    while ($titems.Count -lt 2) { $titems += -1 }
    $treasureRows += ('    {{ {0}, {1} }},   // {2}' -f $titems[0], $titems[1], $n)
    $out.Add("static const StageFoe foes$st[] = {")
    foreach ($f in $foes) { $out.Add(('    {{ {0}, {1}, {{ {2}, {3}, {4} }}, {5}, {6}, {7}, {8}, {9}, {10} }},' -f $f[0], $f[1], $f[2], $f[3], $f[4], $f[5], $f[6], $f[7], $f[8], $f[9], $f[10])) }
    $out.Add('};')
    $out.Add("static const StageThing things$st[] = {")
    foreach ($t in $objs) { $out.Add(('    {{ {0}, {{ {1}, {2}, {3} }}, {4}, {5} }},' -f $t[0], $t[1], $t[2], $t[3], $t[4], $t[5])) }
    if ($objs.Count -eq 0) { $out.Add('    { 0, { 0, 0, 0 }, -1, 0 },') }
    $out.Add('};')
    $out.Add("static const StageEvent events$st[] = {")
    for ($i = 0; $i -lt $evs.Count; $i++) { $e = $evs[$i]; $out.Add(('    {{ {0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {{ {9}, {10} }} }},   // {11}' -f $e[0], $e[1], $e[2], $e[3], $e[4], $e[5], $e[6], $e[7], $e[8], $e[9], $e[10], $i)) }
    $out.Add('};')
    $out.Add("static const StageOp ops$st[] = {")
    foreach ($o in $ops) { $out.Add(('    {{ {0}, {1}, {2} }},' -f $o[0], $o[1], $o[2])) }
    if ($ops.Count -eq 0) { $out.Add('    { 0, 0, 0 },') }
    $out.Add('};')
    $out.Add("static const s16 evpoints$st[][3] = {")
    foreach ($e in $epts) { $out.Add(('    {{ {0}, {1}, {2} }},' -f $e[0], $e[1], $e[2])) }
    if ($epts.Count -eq 0) { $out.Add('    { 0, 0, 0 },') }
    $out.Add('};')
    $out.Add("static const s16 evwalls$st[][6] = {")
    foreach ($e in $ewalls) { $out.Add(('    {{ {0}, {1}, {2}, {3}, {4}, {5} }},' -f $e[0], $e[1], $e[2], $e[3], $e[4], $e[5])) }
    if ($ewalls.Count -eq 0) { $out.Add('    { 0, 0, 0, 0, 0, 0 },') }
    $out.Add('};')
    $al = if ($ally) { '1, {{ {0}, {1}, {2} }}' -f $ally[0], $ally[1], $ally[2] } else { '0, { 0, 0, 0 }' }
    $rows += ('    {{ {0}, foes{1}, {{ {2}, {3}, {4} }}, {5}, {6}, {7}, things{1}, {8}, events{1}, ops{1}, {9}, evpoints{1}, {10}, evwalls{1} }},   // {11}' -f $foes.Count, $st, $start[0], $start[1], $start[2], $start[3], $al, $objs.Count, $evs.Count, $epts.Count, $ewalls.Count, $n)
}
$out.Add('')
$out.Add('// 各ステージの宝（stage_info.treasure、順に）がくれるもの：アイテムの ID（item_base_info.bin）、-1 なし。')
$out.Add('const s16 stage_treasure_item[STAGE_COUNT][2] = {')
foreach ($r in $treasureRows) { $out.Add($r) }
$out.Add('};')
$out.Add('')
$out.Add('const StageFoes stage_foes[STAGE_COUNT] = {')
foreach ($r in $rows) { $out.Add($r) }
$out.Add('};')

# 敵の種類（敵のレコードの変種のバイトで1つ選ぶ）：3d_base/ch_param_N.bin（N = レコードのタイプ：
# 2 ガナード、3 教徒、4 チェーンソー男）、セクション1。最初のブロック：u32 n、n x 108バイトの体の
# レコード（+0x0c：部位ごとのダメージのパーセント s16 x 6、+0x1e：部位ごとのひるみのパーセント u8 x 6、
# +0x42：よろめくひるみの合計 s16。部位は0 頭、1 胴、2-3 腕、4 脚、5 寄生体）。2つ目：u32 n、
# n x 26バイトの種類（+0：u8 体のレコード、+1：u8 装備のカテゴリ、+4：u16 体力、+10：u8 集中、
# +12：s16 duty：AI の敵意。+22：リーダー、GM_Data_ChP_Info::Get_Idv_LeaderFlg。+25：プラーガ付き、
# Get_Idv_WithPlgFlg）。bio4_brew.mod で読んだ：読み込みは 0x3d4e0 と 0x3d18c、種類の準備は 0xd9bc、
# 射撃は 0x22a78（ダメージ = 火力 x ダメージのパーセント / 100 を体力から、0x2b4a4。火力 x ひるみの
# パーセント / 100 をひるみの合計に、レコードの値に達するとよろめく、0x28580）。ハンドガンの火力は100。
$out.Add('')
foreach ($k in @(@(2, 'ganado'), @(3, 'cultist'), @(4, 'chainsaw'))) {
    $b = [IO.File]::ReadAllBytes("$files\3d_base\ch_param_$($k[0]).bin")
    $s = I32 $b 4
    $bo = $s + (I32 $b ($s + 4)); $ko = $s + (I32 $b ($s + 8))
    $kn = I32 $b $ko
    $width = I32 $b ((I32 $b 20) + 8)   # モデルの欄：MoveCol_R（ワード2）、体の幅
    $out.Add("const FoeKind foe_kind_$($k[1])[] = {   // ch_param_$($k[0]).bin")
    for ($i = 0; $i -lt $kn; $i++) {
        $p = $ko + 4 + $i * 26
        $rec = $bo + 4 + $b[$p] * 108
        $hp = [BitConverter]::ToUInt16($b, $p + 4)
        $dm = @(0, 1, 4) | % { [BitConverter]::ToInt16($b, $rec + 12 + $_ * 2) }
        $fl = @(0, 1, 4) | % { $b[$rec + 30 + $_] }
        $fmax = [BitConverter]::ToInt16($b, $rec + 66)
        # 頭へのクリティカル（bio4_brew.mod 0x22be0）：+0x24 頭の確率、+0x38 4つの武器のカテゴリ
        # （s8。1 ナイフ、2 ハンドガン、3 マグナム、5 ライフル）と +0x3c その倍率。
        $cm = @(1, 2, 3, 5) | % { $c = $_; $m = 1; for ($q = 0; $q -lt 4; $q++) { if ($b[$rec + 0x38 + $q] -eq $c) { $m = $b[$rec + 0x3c + $q] } }; $m }
        $out.Add(('    {{ {0}, {1}, {{ {2}, {3}, {4} }}, {{ {5}, {6}, {7} }}, {9}, {10}, {{ {11}, {12}, {13}, {14} }}, {15}, {16}, {17}, {18}, {19}, {20}, {21}, {22}, {23} }},   // {8}' -f $hp, $fmax, $dm[0], $dm[1], $dm[2], $fl[0], $fl[1], $fl[2], $i, $b[$p + 1], $b[$rec + 0x24], $cm[0], $cm[1], $cm[2], $cm[3], $b[$p + 10], [BitConverter]::ToInt16($b, $p + 12), (I32 $b ($rec + 0x4c)), (I32 $b ($rec + 0x58)), (I32 $b ($rec + 0x5c)), (I32 $b ($rec + 0x68)), $width, $b[$p + 22], $b[$p + 25]))
    }
    $out.Add('};')
    $out.Add("const int foe_kind_$($k[1])_count = $kn;")
}
Set-Content "$Root\ds\source\stage_foes.c" $out -Encoding utf8

