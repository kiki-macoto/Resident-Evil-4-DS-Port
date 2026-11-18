param(
    [Parameter(Mandatory)] [string] $InputPath,
    [string] $OutDir = 'ds',
    [switch] $NoMusic,
    [switch] $OnlyMusic,
    [switch] $NoSources
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$tool = Join-Path $PSScriptRoot 're4zeebo.ps1'
$files = Join-Path $PSScriptRoot 'out/files'
$data = Join-Path $OutDir 'data'
$nitro = Join-Path $OutDir 'nitrofs'
New-Item -ItemType Directory -Force $data, $nitro | Out-Null

if (-not (Test-Path $files)) {
    Write-Host '== Unpacking the game (re4zeebo/out/files)'
    & pwsh -NoProfile -File $tool -InputPath $InputPath -OutDir (Join-Path $PSScriptRoot 'out') -Unpack
    if ($LASTEXITCODE) { throw 'unpacking failed' }
}

function X([string]$out, [string]$a) {
    Write-Host "== $a"
    $q = { param($s) $s.Replace("'", "''") }
    & pwsh -NoProfile -Command "& ([Management.Automation.WildcardPattern]::Escape('$(& $q $tool)')) -InputPath '$(& $q $files)' -OutDir '$(& $q $out)' $a" 2>&1 |
        Where-Object { "$_" -match '^\s*\w+: |xception|rror|arning' } | ForEach-Object { "   $_" }
    if ($LASTEXITCODE) { throw "export failed: $a" }
}

if ($OnlyMusic) {
    X $data "-DsMusic 'music=common/snd:st001,st003'"
    return
}

X $data "-DsModel 'leon=3d_base/ch_leon.mcms','gun=3d_base/eqp_wep_all.mcms#1','ganado=3d_base/ch_gna.mcms'"
X $data "-DsAnim 'leon_anim=3d_base/ch_leon_base.mcas#22,23,24,11,20,21,13,3,10,8,16,0,1,2','leon_gun=3d_base/ch_leon_w01.mcas#0,1,2,8,6~4@50,6~4@90,6~4@10,5,7,3','ganado_anim=3d_base/ch_gna_base.mcas#6,7,12,21,9,8,10'"
X $data "-DsModel 'door=3d_base/fix_all.mcms#1','ladder=3d_base/fix_all.mcms#2','barrel=3d_base/fix_all.mcms#3','crate=3d_base/fix_all.mcms#4','medal=3d_base/fix_all.mcms#8' -VramBudget 8000"
X $data "-DsModel 'bell=3d_base/fix_all.mcms#9','tank=3d_base/fix_all.mcms#6' -VramBudget 8000"
X $data "-DsModel 'hang=3d_base/fix_all.mcms#5' -VramBudget 8000"
X $data "-DsModel 'sickle=3d_base/eqp_wep_all.mcms#14','axe=3d_base/eqp_wep_all.mcms#10','fork=3d_base/eqp_wep_all.mcms#11' -DsAnim 'gna_w0=3d_base/ch_gna_w00.mcas#0,1,2,3,4,5','gna_w1=3d_base/ch_gna_w01.mcas#0,1,2' -VramBudget 12000"
X $data "-DsModel 'cha=3d_base/ch_cha.mcms' -DsAnim 'cha_anim=3d_base/ch_cha.mcas#0,1,2,3,4,5,6'"
X $data "-DsModel 'knife=3d_base/eqp_wep_all.mcms#0' -DsAnim 'leon_knife=3d_base/ch_leon_w00.mcas#0,1,2,3,4,5,6' -VramBudget 8000"
if (Test-Path 'assets/title/title_ds_edition_source.png') {
    X $data "-Ds2D 'title=file:assets/title/title_ds_edition_source.png@0,0,1109,832>256,192'"
} else { Write-Warning 'assets/title/title_ds_edition_source.png is missing: title.bin not made' }
X $data "-Ds2D 'hud=common/2d_gms_00.mct@128,0,128,128','stagesel=common/2d_stage_00.mct@0,0,256,224','result=common/2d_gm_00.mct@0,48,256,192','menu=common/2d_gms_01.mct@0,0,256,128'"
X $data "-Ds2D 'items=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24&common/2d_itemic_00.mct@96,96,32,24!cut32x24|common/2d_itemic_00.mct@128,0,128,24!cut32x24'"
X $data "-Ds2D 'star=3d_base/fix_all.mcts#0@0,0,64,64!glow','beam=3d_base/fix_all.mcts#7@0,0,32,64!glow','spark=3d_base/eff_all.mcts#0@0,64,64,64!glow','boom=3d_base/eff_all.mcts#0@96,0,32,32!glow'"
X $data "-Ds2D 'vitalbmp=common/2d_bks_01.mct@0,0,240,96|common/2d_bks_00.mct@192,0,64,84|common/2d_bks_01.mct@0,128,240,32!bmp'"
X $data "-Ds2D 'iconbmp=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24|common/2d_itemic_00.mct@128,0,128,24!cut32x24!bmp'"
X $data "-DsFont 'font=common/2d_fnt_00.mct;common/2d_fnt.hfi;ds/source/text.h'"
X $data "-DsSound 'sfx=common/snd:wep15_shot5,fn57_kara,wep15_reload_re,bogan_kabe,v_damege03,v_fd1,ikaku01,kazekiri,zea_c_down,leon_dam1,leon_dam2,escape_hit,sub_kettei2,itemget_fixeq2,demo_kane,door_w01_kick,tate_break1,handbomb,knife'"
if (-not $NoMusic) { X $data "-DsMusic 'music=common/snd:st001,st003'" }

$env:RE4_MAXEDGE = '100000'
foreach ($n in 1..12) {
    $id = $n.ToString('00')
    $arc = if ($n -eq 1) { '3d_stg01' } else { "3d_stg${id}_00" }
    X $nitro "-DsModel 'stage$id=$arc/stg_$id.mcms' -DsGround 'ground$id=stage$id+$arc/stg_data_$id.bin' -VramBudget 215000"
}
Remove-Item Env:RE4_MAXEDGE
X $nitro "-DsModel 'ash=3d_stg05_00/ch_ash.mcms' -DsAnim 'ash_anim=3d_stg05_00/ch_ash.mcas#0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15' -VramBudget 40000"
X $nitro "-DsModel 'elg=3d_stg04_00/ch_elg.mcms' -DsAnim 'elg_anim=3d_stg04_00/ch_elg.mcas#0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15' -VramBudget 40000"
X $nitro "-DsModel 'jya=3d_stg06_00/ch_jya.mcms' -DsAnim 'jya_anim=3d_stg06_00/ch_jya_base.mcas#6,7,12,21,9,8,10','jya_w0=3d_stg06_00/ch_jya_w00.mcas#0,1,2,3,4,5' -VramBudget 40000"
X $nitro "-DsModel 'gal=3d_stg08_00/ch_gal.mcms' -DsAnim 'gal_anim=3d_stg08_00/ch_gal.mcas#0,2,4,6,8,10,11,14,16,17,18,19,20,21,22,23' -VramBudget 20000"
X $nitro "-DsModel 'cboard=3d_base/eqp_wep_all.mcms#12','cxbow=3d_base/eqp_wep_all.mcms#13','cstaff=3d_base/eqp_wep_all.mcms#15','bolt=3d_base/eff_all.mcms#17' -DsAnim 'jya_w1=3d_stg06_00/ch_jya_w01.mcas#0,1,2,3,4,5','jya_w2=3d_stg06_00/ch_jya_w02.mcas#0,1,2,3,4,5,6,7,8' -VramBudget 12000"
X $nitro "-DsModel 'plg=3d_base/eqp_plg_all.mcms#0' -DsAnim 'plg_anim=3d_base/eqp_plg_all.mcas#0,1,2,3,4' -VramBudget 8000"
X $nitro "-DsModel 'ver=3d_stg12_00/ch_ver.mcms' -DsAnim 'ver_anim=3d_stg12_00/ch_ver.mcas#0,1,2,3,4,6,9,10,12,16,17,18,20,22,24' -VramBudget 40000"
$gm = { param($list) ($list | ForEach-Object { "'gun$_=3d_base/eqp_wep_all.mcms#$_'" }) -join ',' }
$ga = (2, 4, 5, 6 | ForEach-Object { "'gun${_}_anim=3d_base/ch_leon_w0$_.mcas#0,1,2,8,6~4@50,6~4@90,6~4@10,5,7,3'" }) -join ','
X $nitro "-DsModel $(& $gm 2,4,5,6) -DsAnim $ga -VramBudget 8000"
X $nitro "-DsModel $(& $gm 4,5,6) -VramBudget 8000"
X $nitro "-DsModel 'gun3=3d_base/eqp_wep_all.mcms#3' -DsAnim 'gun3_anim=3d_base/ch_leon_w03.mcas#0,1,2,6,3,3,3,7,8,5' -VramBudget 8000"

if (-not $NoSources) {
    Write-Host '== gen_stage_foes.ps1'; & ([Management.Automation.WildcardPattern]::Escape((Join-Path $PSScriptRoot 'gen_stage_foes.ps1'))) -Root $root
    Write-Host '== gen_stage_maps.ps1'; & ([Management.Automation.WildcardPattern]::Escape((Join-Path $PSScriptRoot 'gen_stage_maps.ps1'))) -Root $root
    Write-Host '== gen_shop_tune.ps1'; & ([Management.Automation.WildcardPattern]::Escape((Join-Path $PSScriptRoot 'gen_shop_tune.ps1')))
    if (Test-Path (Join-Path $PSScriptRoot 'cutins_en.txt')) {
        Write-Host '== gen_cutins.ps1'; & ([Management.Automation.WildcardPattern]::Escape((Join-Path $PSScriptRoot 'gen_cutins.ps1')))
    }
}
Write-Host 'Done.'
