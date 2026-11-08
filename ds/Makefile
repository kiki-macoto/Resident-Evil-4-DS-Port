# RE4 Zeebo -> DS demo. Built with BlocksDS (see ../build.ps1 for the Docker wrapper).

NAME		:= re4ds
GAME_TITLE	:= RE4 Zeebo DS demo
GAME_SUBTITLE	:= Research build
GAME_AUTHOR	:= personal

SOURCEDIRS	:= source
# Converted models, animations and ground, embedded with bin2c. Regenerate with:
#   re4zeebo.ps1 -InputPath <zip> -OutDir ds/data
#     -DsModel leon=3d_base/ch_leon.mcms,gun=3d_base/eqp_wep_all.mcms#1,ganado=3d_base/ch_gna.mcms
#     -DsAnim 'leon_anim=3d_base/ch_leon_base.mcas#0,1,2,11,20,21,13,3,10,8,16', 'leon_gun=3d_base/ch_leon_w01.mcas#0,1,2,8,6~4@50,6~4@90,6~4@10,5,7,3', 'ganado_anim=3d_base/ch_gna_base.mcas#6,7,12,21,9,8,10'
#     -DsModel (gimmicks, -VramBudget 8000) 'door=3d_base/fix_all.mcms#1', 'ladder=3d_base/fix_all.mcms#2', 'barrel=3d_base/fix_all.mcms#3', 'crate=3d_base/fix_all.mcms#4', 'medal=3d_base/fix_all.mcms#8', 'bell=3d_base/fix_all.mcms#9', 'tank=3d_base/fix_all.mcms#6', 'hang=3d_base/fix_all.mcms#5',
#           'sickle=3d_base/eqp_wep_all.mcms#14', 'axe=3d_base/eqp_wep_all.mcms#10', 'fork=3d_base/eqp_wep_all.mcms#11' (-VramBudget 12000)
#     -DsModel cha=3d_base/ch_cha.mcms -DsAnim 'cha_anim=3d_base/ch_cha.mcas#0,1,2,3,4,5,6'   (loaded only while he is on the stage)
#     -DsModel knife=3d_base/eqp_wep_all.mcms#0 -DsAnim 'leon_knife=3d_base/ch_leon_w00.mcas#0,1,2,3,4,5,6'
#     -DsAnim 'gna_w0=3d_base/ch_gna_w00.mcas#0,1,2,3,4,5', 'gna_w1=3d_base/ch_gna_w01.mcas#0,1,2'
#     -Ds2D 'boom=3d_base/eff_all.mcts#0@96,0,32,32!glow'
#     -Ds2D 'title=file:assets/title/title_ds_edition_source.png@0,0,1109,832>256,192', 'hud=common/2d_gms_00.mct@128,0,128,128',
#           'stagesel=common/2d_stage_00.mct@0,0,256,224', 'result=common/2d_gm_00.mct@0,48,256,192', 'menu=common/2d_gms_01.mct@0,0,256,128',
#           'items=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24|common/2d_itemic_00.mct@128,0,128,24!cut32x24', 'star=3d_base/fix_all.mcts#0@0,0,64,64!glow',
#           'beam=3d_base/fix_all.mcts#7@0,0,32,64!glow', 'spark=3d_base/eff_all.mcts#0@0,64,64,64!glow',
#           'vitalbmp=common/2d_bks_01.mct@0,0,240,96|common/2d_bks_00.mct@192,0,64,84|common/2d_bks_01.mct@0,128,240,32!bmp', 'iconbmp=common/2d_itemic_00.mct@0,0,128,72!cut32x24!bmp'
#     -DsFont 'font=common/2d_fnt_00.mct;common/2d_fnt.hfi;ds/source/text.h'
#     -DsSound 'sfx=common/snd:wep15_shot5,fn57_kara,wep15_reload_re,bogan_kabe,v_damege03,v_fd1,ikaku01,kazekiri,zea_c_down,leon_dam1,leon_dam2,escape_hit,sub_kettei2,itemget_fixeq2,demo_kane,door_w01_kick,tate_break1,handbomb,knife'
#     -DsMusic 'music=common/snd:st001,st003'   (records through Windows' MIDI synth, real time)
# (-Ds2D/-DsFont/-DsSound/-DsMusic can also run on their own, e.g. after changing the menu text in text.h.)
BINDIRS		:= data
# The stages (models and ground, one pair per stage, loaded when the stage is chosen), in
# NitroFS: for NN in 01..12 (archive 3d_stg01, else 3d_stgNN_00):
#   re4zeebo.ps1 -InputPath <zip> -OutDir ds/nitrofs -DsModel 'stageNN=<archive>/stg_NN.mcms'   (one run per stage: -VramBudget is per run)
#     -DsGround 'groundNN=stageNN+<archive>/stg_data_NN.bin' -VramBudget 215000
#   (models are written as triangle strips; the ground takes the game's collision walls and floors)
# Ashley (stages 5, 6), also in NitroFS:
#   re4zeebo.ps1 -InputPath <zip or re4zeebo/out/files> -OutDir ds/nitrofs -DsModel 'ash=3d_stg05_00/ch_ash.mcms'
#     -DsAnim 'ash_anim=3d_stg05_00/ch_ash.mcas#0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15' -VramBudget 40000
# The shop's upgrades (source/shop_tune.c): re4zeebo/gen_shop_tune.ps1
# The other guns, N = 2, 4, 5, 6 (loaded when equipped; the rifle, gun3: 'gun3_anim=3d_base/ch_leon_w03.mcas#0,1,2,6,3,3,3,7,8,5'): -OutDir ds/nitrofs -DsModel 'gunN=3d_base/eqp_wep_all.mcms#N' -DsAnim 'gunN_anim=3d_base/ch_leon_w0N.mcas#0,1,2,8,6~4@50,6~4@90,6~4@10,5,7,3' -VramBudget 8000
# The giant-climb event picture: -OutDir ds/nitrofs -Ds2D 'giantcut=3d_stg04_00/eve_stg04.mcts#2@0,0,256,128'
# El Gigante (stages 4, 11): -DsModel 'elg=3d_stg04_00/ch_elg.mcms' -DsAnim 'elg_anim=3d_stg04_00/ch_elg.mcas#0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15' -VramBudget 40000
# The castle's cultists: -DsModel 'jya=3d_stg06_00/ch_jya.mcms' -DsAnim 'jya_anim=3d_stg06_00/ch_jya_base.mcas#6,7,12,21,9,8,10', 'jya_w0=3d_stg06_00/ch_jya_w00.mcas#0,1,2,3,4,5' -VramBudget 40000
NITROFSDIR	:= nitrofs

include $(BLOCKSDS)/sys/default_makefiles/rom_arm9/Makefile
