# ds/source/shop_tune.c を書く：移植版にある銃の改造、item_tuneup_info.bin から。
# レコード：u16 0x0004、u8 アイテム、u8 性能、u8 レベル、5バイト、u32 値段、0で終わる値の文字列。
param([string]$Files = 're4zeebo/out/files', [string]$Target = 'ds/source/shop_tune.c')
$t = [IO.File]::ReadAllBytes((Join-Path $Files 'common/item_tuneup_info.bin'))
$n = [BitConverter]::ToUInt16($t, 3)
$p = 5
$out = New-Object Collections.Generic.List[string]
$out.Add('// re4zeebo/gen_shop_tune.ps1 が原作の item_tuneup_info.bin から生成。stages.h を参照。')
$out.Add('#include "stages.h"')
$out.Add('')
$out.Add('const ShopTune shop_tune[] = {')
$count = 0
for ($i = 0; $i -lt $n; $i++) {
    $item = $t[$p + 2]; $stat = $t[$p + 3]; $level = $t[$p + 4]
    $price = [BitConverter]::ToInt32($t, $p + 10)
    $q = $p + 14; $text = ''
    while ($t[$q] -ne 0) { $text += [char]$t[$q]; $q++ }
    $p = $q + 1
    if ($level -eq 0 -or $item -lt 4 -or $item -gt 8 -or $stat -notin 0, 3, 5, 6) { continue }
    $value = [int][Math]::Round([double]::Parse($text.Trim(), [Globalization.CultureInfo]::InvariantCulture) * 10)
    $out.Add("    { $item, $stat, $level, $value, $price },")
    $count++
}
$out.Add('};')
$out.Add("const int shop_tune_count = $count;")
[IO.File]::WriteAllLines($Target, $out)
"$count upgrades"
