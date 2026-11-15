<#
.SYNOPSIS
  Makes all the game data the DS build needs from your copy of RE4 Zeebo Edition, in one go:
  ds/data (built into the ROM), ds/nitrofs (loaded while playing) and the generated sources
  (ds/source/stage_foes.c, shop_tune.c, cutins.c).

.EXAMPLE
  ./re4zeebo/export_all.ps1 -InputPath "D:\Downloads\Resident Evil 4 - Zeebo Edition.zip"

.NOTES
  Run from anywhere; paths are taken from the project folder. Needs PowerShell 7.
  1. The zip is unpacked once into re4zeebo/out (files/, plus PNG and OBJ previews), skipped
     when re4zeebo/out/files is already there.
  2. Every export reads re4zeebo/out/files. Each line below is its own run: the texture budget
     (-VramBudget) is shared by the models of one run.
  3. The music is recorded in real time through Windows' MIDI synth (about 1.5 minutes): keep
     everything else quiet meanwhile, or pass -NoMusic and record it later with -OnlyMusic.
  The title picture (assets/title/title_ds_edition_source.png) is yours to provide: without it
  the title sheet is skipped (with a warning) and the ROM will not build.
#>
param(
    [Parameter(Mandatory)] [string] $InputPath,
    [string] $OutDir = 'ds',        # writes <OutDir>/data and <OutDir>/nitrofs
    [switch] $NoMusic,
    [switch] $OnlyMusic,
    [switch] $NoSources             # leave ds/source alone (no stage_foes.c, shop_tune.c, cutins.c)
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location $root
$tool = Join-Path $PSScriptRoot 're4zeebo.ps1'
$files = Join-Path $PSScriptRoot 'out/files'
$data = Join-Path $OutDir 'data'
$nitro = Join-Path $OutDir 'nitrofs'
New-Item -ItemType Directory -Force $data, $nitro | Out-Null

# 1. Unpack.
if (-not (Test-Path $files)) {
    Write-Host '== Unpacking the game (re4zeebo/out/files)'
    & pwsh -NoProfile -File $tool -InputPath $InputPath -OutDir (Join-Path $PSScriptRoot 'out') -Unpack
    if ($LASTEXITCODE) { throw 'unpacking failed' }
}

# One export run in its own PowerShell (as each was made). $a: the tool's arguments as text.
function X([string]$out, [string]$a) {
    Write-Host "== $a"
    & pwsh -NoProfile -Command "& '$tool' -InputPath '$files' -OutDir '$out' $a" 2>&1 |
        Where-Object { "$_" -match '^\s*\w+: |xception|rror|arning' } | ForEach-Object { "   $_" }
    if ($LASTEXITCODE) { throw "export failed: $a" }
}

if ($OnlyMusic) {
    X $data "-DsMusic 'music=common/snd:st001,st003'"
    return
}

# 2. ds/data: in the ROM.
# Leon, his handgun and the Ganado.
X $data "-DsModel 'leon=3d_base/ch_leon.mcms','gun=3d_base/eqp_wep_all.mcms#1','ganado=3d_base/ch_gna.mcms'"
X $data "-DsAnim 'leon_anim=3d_base/ch_leon_base.mcas#0,1,2,11,20,21,13,3,10,8,16','leon_gun=3d_base/ch_leon_w01.mcas#0,1,2,8,6~4@50,6~4@90,6~4@10,5,7,3','ganado_anim=3d_base/ch_gna_base.mcas#6,7,12,21,9,8,10'"
# The stage gimmicks (doors, ladders, barrels, crates, medallions, bells, tanks, the hanging canister).
X $data "-DsModel 'door=3d_base/fix_all.mcms#1','ladder=3d_base/fix_all.mcms#2','barrel=3d_base/fix_all.mcms#3','crate=3d_base/fix_all.mcms#4','medal=3d_base/fix_all.mcms#8' -VramBudget 8000"
X $data "-DsModel 'bell=3d_base/fix_all.mcms#9','tank=3d_base/fix_all.mcms#6' -VramBudget 8000"
X $data "-DsModel 'hang=3d_base/fix_all.mcms#5' -VramBudget 8000"
# The Ganados' weapons and their moves.
X $data "-DsModel 'sickle=3d_base/eqp_wep_all.mcms#14','axe=3d_base/eqp_wep_all.mcms#10','fork=3d_base/eqp_wep_all.mcms#11' -DsAnim 'gna_w0=3d_base/ch_gna_w00.mcas#0,1,2,3,4,5','gna_w1=3d_base/ch_gna_w01.mcas#0,1,2' -VramBudget 12000"
# The chainsaw man, the knife.
X $data "-DsModel 'cha=3d_base/ch_cha.mcms' -DsAnim 'cha_anim=3d_base/ch_cha.mcas#0,1,2,3,4,5,6'"
X $data "-DsModel 'knife=3d_base/eqp_wep_all.mcms#0' -DsAnim 'leon_knife=3d_base/ch_leon_w00.mcas#0,1,2,3,4,5,6' -VramBudget 8000"
# 2D: the title, HUD, menus, items, effects, the bottom screen.
if (Test-Path 'assets/title/title_ds_edition_source.png') {
    X $data "-Ds2D 'title=file:assets/title/title_ds_edition_source.png@0,0,1109,832>256,192'"
} else { Write-Warning 'assets/title/title_ds_edition_source.png is missing: title.bin not made' }
X $data "-Ds2D 'hud=common/2d_gms_00.mct@128,0,128,128','stagesel=common/2d_stage_00.mct@0,0,256,224','result=common/2d_gm_00.mct@0,48,256,192','menu=common/2d_gms_01.mct@0,0,256,128'"
X $data "-Ds2D 'items=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24|common/2d_itemic_00.mct@128,0,128,24!cut32x24'"
X $data "-Ds2D 'star=3d_base/fix_all.mcts#0@0,0,64,64!glow','beam=3d_base/fix_all.mcts#7@0,0,32,64!glow','spark=3d_base/eff_all.mcts#0@0,64,64,64!glow','boom=3d_base/eff_all.mcts#0@96,0,32,32!glow'"
X $data "-Ds2D 'vitalbmp=common/2d_bks_01.mct@0,0,240,96|common/2d_bks_00.mct@192,0,64,84|common/2d_bks_01.mct@0,128,240,32!bmp'"
X $data "-Ds2D 'iconbmp=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24|common/2d_itemic_00.mct@128,0,128,24!cut32x24!bmp'"
# The font: the glyphs ds/source/text.h uses (run again after changing the text).
X $data "-DsFont 'font=common/2d_fnt_00.mct;common/2d_fnt.hfi;ds/source/text.h'"
# Sound effects, in the order of SFX_* in ds/source/sfx.h.
X $data "-DsSound 'sfx=common/snd:wep15_shot5,fn57_kara,wep15_reload_re,bogan_kabe,v_damege03,v_fd1,ikaku01,kazekiri,zea_c_down,leon_dam1,leon_dam2,escape_hit,sub_kettei2,itemget_fixeq2,demo_kane,door_w01_kick,tate_break1,handbomb,knife'"
# Music, in the order of MUSIC_* in ds/source/music.h (recorded in real time).
if (-not $NoMusic) { X $data "-DsMusic 'music=common/snd:st001,st003'" }

# 3. ds/nitrofs: loaded while playing.
# The stages: model and ground, one run each (big triangles not cut: RE4_MAXEDGE).
$env:RE4_MAXEDGE = '100000'
foreach ($n in 1..12) {
    $id = $n.ToString('00')
    $arc = if ($n -eq 1) { '3d_stg01' } else { "3d_stg${id}_00" }
    X $nitro "-DsModel 'stage$id=$arc/stg_$id.mcms' -DsGround 'ground$id=stage$id+$arc/stg_data_$id.bin' -VramBudget 215000"
}
Remove-Item Env:RE4_MAXEDGE
# Ashley, El Gigante, the cultists, the Garrador, the Verdugo.
X $nitro "-DsModel 'ash=3d_stg05_00/ch_ash.mcms' -DsAnim 'ash_anim=3d_stg05_00/ch_ash.mcas#0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15' -VramBudget 40000"
X $nitro "-DsModel 'elg=3d_stg04_00/ch_elg.mcms' -DsAnim 'elg_anim=3d_stg04_00/ch_elg.mcas#0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15' -VramBudget 40000"
X $nitro "-DsModel 'jya=3d_stg06_00/ch_jya.mcms' -DsAnim 'jya_anim=3d_stg06_00/ch_jya_base.mcas#6,7,12,21,9,8,10','jya_w0=3d_stg06_00/ch_jya_w00.mcas#0,1,2,3,4,5' -VramBudget 40000"
X $nitro "-DsModel 'gal=3d_stg08_00/ch_gal.mcms' -DsAnim 'gal_anim=3d_stg08_00/ch_gal.mcas#0,2,4,6,8,10,11,14,16,17,18,19,20,21,22,23' -VramBudget 20000"
X $nitro "-DsModel 'ver=3d_stg12_00/ch_ver.mcms' -DsAnim 'ver_anim=3d_stg12_00/ch_ver.mcas#0,1,2,3,4,6,9,10,12,16,17,18,20,22,24' -VramBudget 40000"
# The other guns (2 shotgun, 3 rifle, 4 TMP, 5 magnum, 6 rocket launcher) and Leon's moves with them.
# (The shotgun shares its run's texture budget with the other three, which then get theirs again.)
$gm = { param($list) ($list | ForEach-Object { "'gun$_=3d_base/eqp_wep_all.mcms#$_'" }) -join ',' }
$ga = (2, 4, 5, 6 | ForEach-Object { "'gun${_}_anim=3d_base/ch_leon_w0$_.mcas#0,1,2,8,6~4@50,6~4@90,6~4@10,5,7,3'" }) -join ','
X $nitro "-DsModel $(& $gm 2,4,5,6) -DsAnim $ga -VramBudget 8000"
X $nitro "-DsModel $(& $gm 4,5,6) -VramBudget 8000"
X $nitro "-DsModel 'gun3=3d_base/eqp_wep_all.mcms#3' -DsAnim 'gun3_anim=3d_base/ch_leon_w03.mcas#0,1,2,6,3,3,3,7,8,5' -VramBudget 8000"

# 4. Generated sources (and the story scenes' pictures, into ds/nitrofs).
if (-not $NoSources) {
    Write-Host '== gen_stage_foes.ps1'; & (Join-Path $PSScriptRoot 'gen_stage_foes.ps1') -Root $root
    Write-Host '== gen_shop_tune.ps1'; & (Join-Path $PSScriptRoot 'gen_shop_tune.ps1')
    if (Test-Path (Join-Path $PSScriptRoot 'cutins_en.txt')) {
        Write-Host '== gen_cutins.ps1'; & (Join-Path $PSScriptRoot 'gen_cutins.ps1')
    }
}
Write-Host 'Done.'
