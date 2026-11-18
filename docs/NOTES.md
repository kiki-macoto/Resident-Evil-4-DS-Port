# RE4 Zeebo → DS demo

A Nintendo DS / DSi demo built from the *Resident Evil 4 Zeebo Edition* data:
Stage 1 (the village) and Leon, with his animations, the handgun, floor/wall collision,
fog and an over-the-shoulder camera — and five Ganados who walk up to Leon, stagger when
shot and go down.

**Enemy health and hits** (Ganados, cultists, the chainsaw man; the game's own numbers): each
enemy record's variant byte picks a kind in `3d_base/ch_param_N.bin` (N = its type), which gives
its health (Ganados 400-800, stage 1's 400 and 480; cultists 300-1100; the chainsaw man 3000) and,
through its body record, by part (head / body / legs) the percent of a shot's firepower it loses
(Ganado 120 / 100 / 90) and the percent added to its flinch total (150 / 80 / 100): it staggers
only when that total reaches its record's (Ganado 150; cultist 200, chainsaw man 240), so a
Ganado staggers at every head shot and every second body shot, and otherwise keeps coming.
Firepower is the game's (handgun 100, x 10 the port's `power`). `item_weapon_info.bin`: "WPI", u16 16 weapons, u16 7 levels, then 30-byte records (weapon x 7 + level; `Bio4_ItemInfo::GetWeapon`), u16 near and far firepower first: knife (weapon 0) 130, handgun 100, Magnum 1300, rifle 500, TMP 40 (the shotgun 120 / 80; the port keeps the tune-up table's 400). Leon's kick is 200 (`ch_param_0` scripts 31 and 36, attack command 103), the knife 130: both in. Traced in `bio4_brew.mod` (see
`gen_stage_foes.ps1`, which writes the tables into `stage_foes.c`). Not from the game: legs are
hits below 7.5 of 17; breaking free takes 200. The game's critical head
shot (a chance by weapon of x10 damage, 0x22c04) is not in: the weapon's chance was not found.

**Leon's health in the game**: 1200, +120 for each yellow herb, at most 2400 (0x10d58; Ashley 600,
+60, at most 1200, 0x10d3c). A green herb gives back 600 (`item_healing_info.bin`: 5-byte
entries, u16, u8 kind, u16 amount, ffff = all; the first is 600), so the port's herb gives 50 of
its 100.

**The Ganados' attack damage** is in their animation scripts. Neither the Zeebo module nor the
Android data could show it (the module keeps no names, the Android data files are encrypted).
It was read in the Android release's library, `libbio4af.so`, whose symbols are kept (the same
engine: `GM_*`, `Bio4_*`). `ch_param_N.bin` is five parts, at the header's words 1-5: info (the
body and kind records above), motion table, animation scripts, model nodes (the old "section
4": node radii, not damage), model fields (`GM_Data_ChParam::Init`).
- Scripts (`_Make_AnimScpData`, `GM_Data_AnimScript::Init` / `Read_Data`): u16 count, u16
  commands per script, then per command u16, u16 first frame, u16 last frame, u16 id, s32 x 10.
- Ids under 100 are common commands, 100 and up the character's (`GM_Ctrl_ObjScriptCh::doInit`,
  `GM_Ctrl_ObjScriptEv::Main`): 103 a blow (`_Cmd_Ch_AtkNor`), 106 a hold attack.
- A blow's values 6, 7, 8 go to `_Set_NearAttack` → `_Hit_NearAttack` → the victim's
  `Set_AimDamage`, where value 6 times the victim's damage % per body part / 100 is taken off
  (`GM_Obj_ChBase::Set_AimDamage`, `Sub_Is_Hp`), and value 7 is the push. Leon's damage % is 100
  on every part, Ashley's 50.
- Motion table (`GM_Data_ChP_MvtTbl::Init`): u32 count, u32 categories, then 48-byte entries:
  s8 category, u8 type, u8, 5 x {u8 condition, s16 x 3}, s16 script x 5. Conditions are indexed
  in `GM_CtrlSub_ChHandler::Init`'s order (8 the usual pick, 25 bare-handed, 27 the
  harder-difficulty variant).
- The motion table's category is the attacker's equipment category (kind +1, `EqpCtg`), and
  `GM_Data_ChParam::Get_WepMdlId` gives its weapon's `eqp_wep_all` model: 13 #10 (the hatchet),
  14 #11 (the pitchfork), 17 #14 (the sickle), 19 #16. A mission record's kind thus arms its
  Ganado: kinds 0, 3, 6 ... bare hands, 1, 4 ... a hatchet, 2, 5 ... a pitchfork, 18 and 37 (3000
  health) category 19.
- Ganado (`ch_param_2.bin`): bare hands don't strike: they reach out for a hold (script 41) or
  lunge for one (42-44) and then bite, 50 twice a loop (45; 70 harder); the hatchet 380 (62-64;
  480 harder), the pitchfork 380 with a push of 150 (75-77), category 19 800 (84-90; 1000
  harder). (Read at first as bare hands 380, one-handed 380 and the pitchfork 800: corrected.)
- Others: cultist sickle 700 (script 62) or 800 (65; the port's), chainsaw man's hold up to 1600 (his kill), Garrador 640 falling to
  160 over a combo, Verdugo 650 (700). In the Android release Leon's damage % is also lowered
  by armour and difficulty (`GM_CtrlParam::Set_Start`); the Zeebo has neither.
- **Where a blow lands** (attack command 103's values 1-5; `_Check_NearAtk_isTrgAreaIn`): it
  hits if the target's distance across the floor is under value 1 + the target's width
  (`Get_MdlWidth` = its `MoveCol_R`, model fields part, word 2: Leon 1000, a Ganado 1200),
  its height band overlaps values 2-3, and it is within the angles 4-5 (65536 a turn). The
  blow is checked on the script frames it is listed for, after the script's own moves (command
  4) have carried the attacker forward. The hatchet (script 62): 3200, 1000-5600,
  -1200..3200, on frame 13, after stepping 90 a tick on frames 4-9 and 270 on 10-16 (about 6
  port units by the blow); so it reaches about 16.4 port units from where it then stands. The
  pitchfork (75): 5120; category 19 (85-90): 6000-7400, swept across four frames. A hold (command
  106) is checked the same way on each of its frames: 2500, 3500-4000, +-1000. What starts an
  attack is the motion table's condition in its third slot, whose ids (up to 117) go past the
  Android library's 36; their first value reads as a distance in port units, and it fits each
  attack (in reach after its step): hatchet 12, pitchfork 22, the hold 12, the lunge 30 within
  +-6400 (35 degrees). **In the port** (enemy.c `attacks[]`): each attack plays its script on its
  clip's frames (the hatchet now ch_gna_w00 #4, as the script has it; the hold #9, the lunge #8,
  the pitchfork w01 #2): turning to Leon, its steps and its hits, with these distances and
  areas; it starts at the condition's distance (only that reading is the port's own). Cultists
  likewise (their holds as the Ganados', their sickle script 65).
- **One attacker at a time** is the game's too: `GM_CtrlParam::Set_AiCondFlg_EnAtkOn` (set in
  `GM_Hdl_ChBase::Main_State_Attack`, cleared in each handler's `Main`) is a flag per enemy that
  the AI (`GM_CtrlSub_ChAi::_Set_UpDateParam`) reads. No fixed pause between attacks was found:
  when a Ganado attacks comes out of the AI and the motion table's conditions (enmity, anger).
- Big enemies pick between two scripts by a state (condition 21: 520 and 539); the port takes
  the 520 one: Garrador 640 (33), El Gigante 800 (16-25; 2400 in 14-15), Verdugo 650 (12-40).
**Leon's moves**: the game's motion table (`ch_param_0` motions 1-12) plays the standing pose #22, walk #23 and run #24 of `ch_leon_base` while his health is 800 of 1200 or more, the hurt ones #0-2 below (conditions 12 HpMore 800, 11 HpUnder 799; walking back is the walk backwards). The port had used #0-2 throughout; it now switches at 67 of 100. `-DAUTOPILOT=61` stands, walks and runs.

**Against the game (the Android release's library and the Zeebo module)**: in step with it
now: enemy health, damage and flinch by part; their blows, holds, steps, reach and damage
(attacks[]); weapons by kind; walking and turning speeds; the bosses' health, damage %,
flinch and blows; Leon's health, herbs, vest, knife and kick; his healthy / hurt moves; drops
(codes, rounds, money, treasures) and taking them with A; critical head shots (below); when
they attack (enmity); the hold, its bites and breaking free; what they notice (below). Still the
port's own, to bring in line (scope: the game's workings and its screens, not the Android-only
difficulty modes): Leon's and Ashley's speeds and turning; Ashley's following; the enemies'
Leon's safe time after a hit; when a Ganado is dazed (and the
kick); the throwers; blasts (barrels, canisters, the launcher); the shotgun's spread; firing
and reload times; piercing; the camera; ladders and falls; how the waves come in and how many
at once; the bosses' moves (the Garrador's hearing, El Gigante's kneeling and the cuts, the
Verdugo's freezing and leaps); what a treasure is; room in the case for what is picked up; and
the screens (HUD, status, shop stock by mission, results) where they differ.

**What they notice** (stages 2-12, while they stand watching; `GM_CtrlSub_ChAi::_Get_SearchParam`
and the search conditions): his distance across the floor less both bodies' widths (`MoveCol_R`)
and 300, and his angle off the facing (`atan2`, 65536 a turn). Seen: within the body record's
look distance and angle (+0x4c, +0x58: a Ganado 63760 and +-1280, a narrow 7 degrees ahead out
to 259 port units; `_SearchCond_Look`), whatever the height. Heard: within its search distance
and angle (+0x5c, +0x68: 17920, +-28550, 157 degrees either side: 80 units) while Leon runs or on a
shot (`_SearchCond_Area_Run`, `_Area_Shot`). Touched: no gap left (`_SearchCond_Touch`). A
hit (`_SearchCond_Damage`). The port had them notice him within 40 whichever way they faced,
or within 150 and 60 degrees ahead. Still the port's own: one that notices tells those within 80
(the game has a friend condition, `_SearchCond_Friend`, whose caller was not traced), and a foe
whose spawn point is far off, brought in near Leon, comes knowing where he is. Stage 1 (the
port's own spawning) keeps them walking at him within 120. `-DTEST_SHOW_SPOTTED` shows how many
noticed x 100 + how many stand (FPS field), without that alert.

**Critical head shots** (bio4_brew.mod 0x22be0): a head hit is critical with a chance of the
kind's head chance (body record +0x24: Ganado 100 or 70, cultist 90 / 60, chainsaw man 20) x the
weapon's critical rate / 100 (``item_weapon_info.bin``, a level's 13th value: handgun 8, Magnum
16, rifle 56, the shotgun and TMP none); it then takes the firepower x the kind's multiplier for
that weapon (+0x38 four weapon categories, +0x3c their multipliers; Ganado: handgun 10, rifle 5,
Magnum 4, knife 4), at most 30000, whatever the part's damage percent: an 8% chance that a
handgun head shot fells a Ganado outright. ``item_weapon_info.bin``'s records (16 bit): near and
far firepower, push, two distances, rounds, two times, then (by the setter order of
``GM_CtrlMain_Equip::Set_WepParamNormal``) piercing, shots, scope, critical rate; the two times'
units against the shop's tune-up values are not settled.
**Speeds and units**: `GM_Ctrl_ObjScript::_Cmd_Move` (base command 4: value 0 the direction, 1 the speed, 2 = 0: plus the character's `MoveSpd`, kind +6) feeds `GM_ObjBase::Set_StaMove`, and `GM_Obj_ChBase::_Main_Move` adds it to the position each tick, without a time step. The Android game runs 20 ticks a second (re4mepc: "the 20 Hz simulation"; the port plays the clips at 20 frames a second too), positions are the mission files' units (1/256 of the port's), so a speed s is s / 768 of a port unit a 60 Hz frame. Walking (script value 0 + `MoveSpd`): Ganado 90 (0.117; the port had guessed 0.11), chainsaw man 130 (0.169; 0.16), El Gigante 240 (0.3125; 0.30), Verdugo 200 (0.26; 0.2): these are in. Turning: `Set_StaTurn` / `Main_StaTurn` add `RotSpd` (kind +8) to the angle each tick, 65536 a turn, so it is RotSpd / 6 libnds units a 60 Hz frame: Ganado and chainsaw man 500 (55 degrees a second; the port had 120), Garrador 900, El Gigante and Verdugo 800: in. Not in yet, as what they come to is not settled: Leon (`MoveSpd` 300: walk 300, run 700, would be 0.39 / 0.91 against the video's 0.27 / 0.60, and his clips' feet), Ashley (300; her moves are capped by `Set_LimitParam`), the runs and charges (Garrador 790, Verdugo 900; also capped), Leon's turning (`RotSpd` 900 x the stick's rate / 100, 99 degrees a second, against the video's 240), and sight and hearing (body record +0x4c: `LookDis`, `LookRotL/R`, `SearchDis`, `SearchRotL/R`; Ganado 63760 / ±1280 and 17920 / ±28550).
- The port (Leon 100 for the game's 1200; Ashley's 50% on her half health comes out the same):
  hatchet and pitchfork 32, bite 8 per loop, cultist sickle 67, Garrador 53, El Gigante 67,
  Verdugo 54 (the chainsaw man still kills outright). Cultists carry a sickle when their kind is
  armed (their other weapons' models aren't in the port).

The Ganados fight back, one at a time, as the game paces them (`GM_CtrlSub_ChAi::_Set_UpDateParam`): each one's enmity gains its kind's concentration a tick (Ganado 8, cultist 14, chainsaw man 20) up to 1000, and first loses its duty (700, 800, 600) while any enemy is attacking; it may attack at 1000 (motion condition 8, EnmityMax). So a Ganado is ready again 6.25 s after the last attack anyone made. A weapon's blow (32 damage; Leon flinches); bare-handed ones reach out for a hold, or lunge for one. Held, he is bitten on two frames of every loop of the hold (script 45: 50 each, 4 here: about 10 health a second); `Mash A: Struggle!` blinks (the game's "Ação: Eu luto") and the 8th press of A/B/X/Y breaks free (`GM_CtrlMain_Player::_State_HoldDamage`: past 7); the Ganado holds on until then (`Main_State_HoldAttack` has no time limit) and is shoved back 3000 (11.7 units), unhurt. (The port had them wait 4-5.5 s each and 1.25 s after anyone's attack, let go after 4 s, bite 8 every other loop, and the breaking free hurt and dazed the Ganado: all gone.) Ganados are solid: Leon can't walk through them.

Screens and HUD use the game's own 2D graphics and font (Japanese text):

- **Title**: the title art (`assets/title/title_ds_edition_source.png`: the Zeebo picture with the subtitle changed to *Nintendo DS edition*; the untouched originals are next to it) with a menu, ゲームを始める (start) / 操作方法 (controls list).
- **Stage select**: the original's 12 stage pictures (`2d_stage_00`) in a grid; the chosen one
  is drawn larger in a pulsing gold frame. Only stage 1 (村, the village) is playable; the others
  are dark, say 準備中 (in preparation) and shake with a click when chosen. Confirming zooms the
  picture to fill the screen, then fades into the stage.
- **Item drops**: a Ganado going down leaves what its mission record's drop code says, a broken
  crate what its point's says (see **Drops** below; nothing for none, ff, as in the game): rounds
  for a gun, a green herb (carried, up to 9; used from the item screen for +50 health: the game's
  600 of Leon's 1200), money or a treasure. It shows as its icon (`2d_itemic_00`: each gun's
  box) floating in a light column with a twinkling star (the original's glow textures, `fix_all`
  #7 and #0, made translucent). Within reach `A: Pick up` shows and A takes it, with sparkles
  (`eff_all`), the pickup sound and a popup ("Shotgun Ammo +5"). As the game does it
  (`GM_CtrlMain_Player::_Check_GetEvent`: an item in reach is taken on the action button;
  `GM_CtrlMain_Field::Check_GetDropItm`: within its radius 2000 + Leon's width 1000 across the
  floor, no more than 2000 below his feet nor his height 4800 + 2000 above: 11.7 port units).
  `-DAUTOPILOT=63 -DTEST_DROP_CODE=n` walks up to a test drop and takes it. Rounds carried (残弾) show above the plate; reloads
  draw on them, Leon starts with 20.
- **Stage clear**: once every Ganado lies dead, the church bell (`demo_kane`) tolls three times
  while the music fades, STAGE CLEAR comes up, then the results (in English): time (with the
  original's TIME counter graphics), enemies, accuracy, head shots, damage taken and a rank
  (S/A/B/C from speed, accuracy and damage). A goes on to the stage select.
- **HUD**: Leon's plate from the original HUD at the bottom right of the top screen, with the
  rounds in the magazine in its counter and his health (100 HP) in its gauge: green FINE,
  yellow CAUTION, red DANGER.
- **Status screen** (SELECT; the game's Status screen, its button 3, opened on its weapon list,
  Armas): rebuilt from the game's layout files on its 320 x 240 screen, scaled by 0.8 about the
  centre (`Bio4_ImageLayout.bin`: 24-byte records of picture sheet, source rectangle, place from
  the centre and drawn size; records 285-293 and 356-402 here; the text from
  `Bio4_StringLayout.bin` 444-485). The frame bars, list panels and attaché case come from
  `2d_gms_01` (the `menu` sheet, 32 KB, loaded only while the screen is open), the red frame
  round the chosen row is drawn as lines (the game's, in `2d_gms_00`, are too thin for the 0.8 scale). Each row: the item's icon (the equipped weapon marked
  E), its name, its count with the ammo mark (not on a herb's row, as in the video) and its size in the case with TAM (sizes from
  `item_base_info.bin`: handgun 6, ammo box and herb 2; boxes hold up to 50 rounds as in
  `item_ammo_info.bin`); four rows at a time, with arrows when there are more. At the bottom the
  money (PTAS) and the case in use / its size (060: the first case of `item_ac_info.bin`), with Leon's life and ammo plate just above. The list's title is Weapons (the game's Armas, as in the video).
  Leon stands on the right holding his gun. A on a row opens its commands in a small window:
  Equip / Cancel for the handgun, Discard / Cancel for ammo, Use / Discard / Cancel for a herb
  (Use heals as on the vital screen). The game's Combine, discarding the only weapon and the
  other tabs (Map, Treasure, Key, File) are left out. B or SELECT goes back to the game.
- **Item select / vital screen** (always on the bottom screen in the game, worked by touch):
  built from the original's case panels (`2d_bks_01`, `2d_bks_00`'s item slot frames), item
  icons and the game font, drawn into a 16-bit bitmap. The vital panel shows Leon's condition
  (FINE / CAUTION / DANGER in green / yellow / red), his health and a heartbeat trace that speeds
  up and reddens as he gets worse. Three slots: the gun in hand (its own item icon and name, changing with the equipped weapon; rounds loaded), Multi Ammo
  (the rounds carried for it, with that gun's ammo icon) and green herbs (count); touch one to select it (red frame, description
  below). Double-tap the herbs to use one (+50 health;
  refused with "体力は満タンだ" when health is full). FPS shows top right. Outside the game the
  bottom screen shows just the backdrop.
- **Pause** (START): the game's own Opções screen, in English. The text, its places and colours
  come from the game's layout file (`common/Bio4_StringLayout.bin`: 20-byte records of string
  number, x and y from the centre of the 320 x 240 screen, size, alignment and the chosen /
  other colours; the strings follow from 0x2C53, each letter a 16-bit glyph code, the ASCII
  ones code - 0xDF; Opções is record 288, its list 300-304), scaled by 0.8 about the centre:
  Options at the top; Retry / Quit / Sound / Controls in white, the chosen one gold; the
  chosen one's help line in pink below (Retry the mission, Return to the title screen, ...).
  Retry and Quit ask (Retry? / Return to the title screen?) with YES / NO in gold / grey,
  starting on NO; Sound picks OFF / Volume: Low / Medium / High (the music and the effects);
  Controls shows the controls. The game's fifth item, Vibration, and its control types
  (TIPO A-D) are left out (nothing to set on the DS). The scene stays dimmed behind it. The
  background picture the game draws there was not identified in its image layout file.
- **Game over**: when Leon dies, YOU ARE DEAD with コンティニュー (continue) / タイトルに戻る.

Sound effects are the game's own (`common/snd/*.wav`, IMA-ADPCM, converted to 8-bit PCM at
22 kHz): handgun shot, dry fire, reload, bullets into walls; Ganado shouts when they spot Leon
and when they attack, the swish of a swipe, their cries when hit and going down (from where the
Ganado is: quieter with distance, panned left/right); Leon's pain cries, the kick that breaks a
grab, and the menu confirm sound.

Music: the game's MIDI tracks, `st001` on the title screens and `st003` in the village
(guessed from their order in the game's music table; it goes quiet in the pause menu and
stops when Leon dies). The DS has no MIDI synthesizer, so the export plays each track through
Windows' General MIDI synth (Microsoft GS Wavetable Synth), records it, and stores it as a
looping IMA-ADPCM sample (16 kHz) that the DS sound hardware plays and loops by itself.

**The castle** (stages 7-12; stage 6 is mission 6, the cabin siege in the village, with Ganados): the enemies are the cultists in red robes (`ch_jya`, from NitroFS; the Ganados' skeleton and clip order, with their own one-handed weapon set). **Throwers**: the mission files' kind-03 points (stage 2's village, and up on the castle walls in stage 7, as the video shows the catapults there) stay where they are and throw at Leon every few seconds: a burning rock (castle) or dynamite arcs to where he stood and goes off (30 damage nearby, Ashley too). Ashley is with Leon in stage 7 as well.

**El Gigante** (stage 4; two in stage 11, as mission 11 has it: the second starts 40 degrees to Leon's left and 30 units farther, sharing the first one's model (`giant_share`); they keep apart, each is shot and cut on its own, and the stage clears when both are down): the game's model and animations (`ch_elg`, from NitroFS; 45 bones, the last 17 folded in). He walks at Leon (240 a 20 Hz tick, the game's) and swings (67 damage, the game's 800; a blow shakes the view). His flinch total brings him to his knees with the parasite out of his back (the game's: each shot adds half its firepower, 1010 drops him: 21 handgun hits): walk up and press **A** (`A: Climb`, the game's "Ação: Escalar"). Leon has no animation of climbing him in the game's data (none of his clips but the ladder climb moves him up); the Zeebo version shows it as a story scene (see **Story scenes** below: scene 11, its picture of Leon on top of El Gigante and two lines). The port does the same; then Leon is behind him, facing him. He also has the game's 4500 health that every shot wears down (its firepower: handgun 100) and a cut takes a third of (the port's own): the rifle can finish him, as in the video. Three cuts fell him too; he stays down 12 s, counted from when he is down (time to run up from where he was shot), and gets up if not cut. Three cuts fell him; the stage clears when he and the Ganados are down. (In the Zeebo video the stage-4 fight is the mine arena with Ganados around, the giant shot until he drops and the parasite cut.)


**Story scenes** (the "cut-ins"): as the Zeebo version tells its story between and inside the missions, a picture (256 x 128, `eve_stgNN.mcts`) over black with lines of text below (drawn into the sub screen's 16-bit bitmap with the screens swapped while it plays, so it needs no texture memory: in stages whose textures fill VRAM the picture used to be missing), who speaks in gold, **A** for the next page (or 8 s), **START** to skip the rest. They come from the mission files' scripts (`mis_data_sNN.bin`: word 5 holds the scenes, 48-byte commands; 201 shows picture k, 200 a page of text, 202 money; word 6 the texts), with the Portuguese lines translated in `re4zeebo/cutins_en.txt`. `re4zeebo/gen_cutins.ps1` writes `ds/source/cutins.c` and exports the 41 pictures used to `nitrofs/eveNN_k.bin` (each loaded only while shown). Played: each mission's opening (scene 0; entered from the stage select, not on a retry; stage 12's is empty, so its Verdugo scene plays instead) and its clear (scene 9, before the results); the chainsaw man coming (stage 1), El Gigante kneeling the first time and Leon on his back (stages 4, 11), Ashley found (stage 5), the Garrador met (stage 8, Leon within 60 units of him). El Gigante in stage 4 pays 15000 PTAS when he falls (the script's money command).
**Load, reach and the real DS** (fixes after play on hardware):
- *Polygons*: the characters are the game's full meshes (an edge-collapse reduction, `-DsModel 'name=file!lodNN'`, tore holes in Leon's and Ashley's thighs on hardware and is not used). Stage chunks wholly off the sides of the view are not sent. Stage 6's cabin: from outside it, its inside is not drawn; from inside, nothing beyond its yard. Stage 8: from the gallery, the hall below (and anyone in it) is not drawn. When the stage is already small and the frame is still full, the farthest enemy is left out first. The laser sight is drawn right after the HUD, so a full frame never drops it.
- *Two-sided walls*: the game's materials say which surfaces are drawn from both sides (byte 0x24 of a material record, 2: walls and parapets seen from inside a room as well as from outside). The exporter writes it into each draw's header (`RDS1`: the byte after `transparent`), and those draws go out with culling off; before, the inside of such walls (stage 6's start, and others) was not drawn at all. (The stage files in `ds/nitrofs` were patched with the flag; exporting them anew with `RE4_MAXEDGE=100000` gives the same draws.)
- *Enemies and walls*: they keep 3.2 off walls (2.2 between each other), pushed out in two passes (corners); the one holding Leon is kept out of the wall behind him; a body that falls is first moved 6 off the walls, so it lies on the floor.
- *Stage chunks*: sent strictly nearest first: once one doesn't fit the triangle budget nothing farther is drawn (skipping only that one let smaller chunks behind it in, so a big wall by the camera vanished with the distance showing through it).
- *Frame time*: frames slower than one vblank for most of half a second cap the stage's triangles (never under 1200) and the enemies drawn lower, step by step (a second at full speed lets them back up), for scenes the real DS runs slower than an emulator.
- *Foes outside the field*: when a foe comes in, a flood fill over the floor from Leon's feet (4-unit steps, a step up or down at most, up to 45 steps; no wall within 2.2 of the new place nor of the point half way to it) tells where he can walk (testing only the places themselves, it had stepped over thin walls between them, and foes came in outside the field: stage 6's start, and every other stage when checked); a mission point it doesn't reach (behind the field's walls, across a gap, down in stage 10's hall) is not used: the foe comes in at a reached place instead. `re4zeebo`-side audit: scratch `spawnaudit.ps1` runs the same fill from each stage's start.
- *Foes that never came*: a foe whose mission point is more than 110 away or on another level (stage 6's across the fields, stage 5's on the gallery, stage 10's in the hall below the balcony) enters instead 18-85 away where Leon can walk straight to, already after him.
- *Throwers* (stages 7, 10): they throw only within 90 and with nothing between them and Leon; a blast costs 20.
- *Stage 12's doors and walls*: its stage data (collision, mission points: doors, start, the Verdugo, tanks, crates, treasures) is moved onto its model by (-468, -988) and then a few units more (`Stage12Fix` in `DsExport.cs`, `Fix12` in `gen_stage_foes.ps1`: x + 6.64 + 0.00374 z, z + 4.9, read off its three doorways in the model; the collision takes half of it, which fits the model's walls best). Before, every door stood 4-7 units off its doorway. A door against a wall still leads on when both its sides have floor clear of walls (stage 12's narrow doorways).
- Ashley's plate shows once Leon has found her. On a ladder whose ledge is nearer than the climb carries him back (stage 5's chapel), Leon is drawn further out as he climbs, not into the wall. Crates on shelves (stage 10) stay where the mission puts them. The Garrador charges at 0.42 (Leon runs 0.60) and turns slower; the Verdugo runs at 0.36, leaps every 14 s, and a tank freezes him within 34.
**Later fixes** (from play on hardware):
- Stage 6 (mission 6, the cabin siege) is left out, and the stages after it are numbered on from 6 (`stage_no()`: the battlements are STAGE 6, the old canal STAGE 11), on the stage select and the mission title.
- The stage select shows one stage at a time: its number and place and the mission's name at the top, its picture (2d_stage_00, twice the size) between red arrows, and the mission's three lines below (the game's mission list, `stgnfo_mission_ms.bin`, in English). Left / Right scroll, the pictures sliding across; A confirms and the picture grows to fill the screen.
- The top screen no longer shows the rounds carried (`AMMO n`): they are on the bottom screen.
- Life bars as in the Zeebo version: Leon's runs long, from x 120 to the plate's right end, black where life is lost, emptying from the left; Ashley's plate sits right above Leon's and empties the same way. The weapons (status) screen shows no life bar, and the list's panels carry on behind Leon's figure to the screen's edge (`overlay_far()` puts them behind the 3D figure).
- Shots: the fire animation's motion from its first frame is added to the aim in full, each joint's turn applied as a rotation after the aim's (`anim_add_delta_rot`): adding it as Euler angles, the shotgun's big pump swung the gun up and over Leon's shoulder.
- The knife follows the view: with L held, Up / Down tilt the view as with the gun, and Leon's upper body leans with it: everything from the waist up turns about his own left-right axis through the waist (35 degrees back at full tilt up, 33 forward down; turned joint by joint inside the twisted knife stance it had bent sideways), and looking up the view comes down behind him and tilts less so he stays in it; the slash reaches 14 along that line from his chest (not through walls), so crates, barrels, bells and medallions up high break, and treasures up high come down; looking level or down, a crate at his feet still breaks.
- Reloading: Leon lowers the gun and can walk (and turn) while the rounds go in, as in the Zeebo version; the reload pose moves only his upper body (nodes 10 up), the legs keep walking. With R still held he aims again once it is done.
- Stage 5's chandelier (the Zeebo version's "Ação: Pular"): the gallery on the far side of the ladder (the south and west arms, with a treasure) lies across the nave. Standing at the north gallery's railing facing the nave (x 64, z 18), `A: Jump` plays the mission's scene 17 (event picture #4: Leon jumps onto the chandelier and swings to the other side) and Leon lands at the stage's point 13 on the south gallery, Ashley just behind him; the same from the south railing brings him back. (The arched opening in the east gallery at z 150 is bricked up in the game's model and collision: not a way through.)
- Doorways: a door that leads on clears the walls across its doorway (`ground_add_gap`): the outline of the room where Ashley waits in stage 5 ran right behind its door, an invisible wall.
- Foes that come in near Leon (those whose point is far off or on another level) are placed by a flood fill over the floor from his feet (12-45 steps of 4 units, through corridors and along stage 10's balcony), at the place farthest from those already standing: they come from all round, not in a crowd.
**The Garrador** (stage 8): the game's model and animations (`ch_gal`, from NitroFS; 33 bones, the last 5 folded in). As in the Zeebo video (a cell in the castle, "he can't see Leon, but he can hear his steps"), he is blind: he stands listening, and charges at any shot, at Leon running within 60 units, or at Leon right beside him. Leon in reach at the end of a charge gets his claws (53 damage, the game's 640); a charge into a wall leaves his claws stuck in it for 2.5 s. His health is the game's 3500: a shot takes all its firepower in his back (the parasite), only 7% elsewhere, so 35 handgun hits in the back fell him; he reels when his flinch total (100% of a shot, 200% in the back) reaches 400. Ashley is with Leon here too (the video shows her by his side past the cell): his claws catch her as well if she is in front of him. (His texture is halved, 64x128, to leave texture memory for her.)

**Missions and their enemies** (the game's mission list, `common/stgnfo_mission_ms.bin`): 1 Sede de Sangue (village), 2 Luis Sera (valley), 3 Las Plagas (waterfall), 4 A Gaiola do Gigante (mine, El Gigante), 5 O Resgate de Ashley (church), 6 A campanha no cerco (the villagers besiege the cabin), 7 Para o Grande Castelo (battlements), 8 A Arma Cega (audience hall, the Garrador), 9 A Dupla de Guardiões (two Garradors), 10 Reunião (central hall), 11 A Fornalha (two El Gigantes), 12 O Assassino mais Forte (old irrigation canal, the Verdugo). Who is in each comes from the mission file's enemy records (see below): the Garradors (one in stage 8, two in stage 9, shared model `garrador_share`), the giants (one in stage 4, two in stage 11) and the Verdugo stand on their records' points.

**Enemies noticing Leon** (stages 2-12): each stands on its spawn point facing the way its record gives it (256 to a turn), instead of every one heading for Leon from anywhere at once. One notices him within 40 units whichever way it faces, or within 150 in front of it (about 60 degrees either side, not far above or below), or when shot; those within 80 of it notice too; once it has noticed him it keeps after him. (Stage 1 keeps its own scheme.) `-DENEMY_SIGHT=inttof32(n)` changes the 150.

**Collision and walls** (all stages): the game's own collision data (`stg_data_NN.bin` section 1) starts one word later than first read: every triangle had been read shifted by one coordinate, so the walls taken from it stood in the wrong places. Read right, its walls block where the game's do and its floors (ledges, walkways, stairs) are added to the ground, except the huge flat planes under whole maps; stage 12's data is moved by the same 468, 988 as its mission points. **Camera**: a ray from Leon's head to the camera brings it in short of any wall between them, so turning with his back to a wall no longer puts the camera behind it (where the wall faces away and isn't drawn). **Vertex budget**: models keep the game's triangle strips (`BEGIN_VTXS` strip, neighbours share vertices) instead of separate triangles, about 30% fewer vertices; the DS stores at most 6144 a frame and 2048 polygons, and stages with Ashley had reached 6144. `-DTEST_GX` shows the load on the vital screen (FPS field: polygons; spare rounds: vertices; herbs: frames over the limit in the last second; Loaded a/b: the stage's own polygons / vertices). `-DAUTOPILOT=20` (+ the barn-wall start) turns Leon round with his back to a wall.

**Drawing speed**: every character model's draw list (texture, polygon format, vertices) is sent to the geometry engine by DMA, in the background, like the stage model; the CPU poses the next character meanwhile, and waits only before anything else is drawn (`model_sync()`). With this, stages 6, 7 and 8 (with Ashley, the cultists and the Garrador) run at 56-60 fps in melonDS (before: about 30, 42-55 and 35-40). The stage clears when he and the cultists are down.

**The Verdugo** (stage 12): the game's model and animations (`ch_ver`, from NitroFS; 49 bones, the last 21, his tail and fingers, folded in). His moves follow the original game: he stalks Leon, running when far, and strikes with his tail (54 damage, the game's 650); every 9 s or so he springs up out of sight and drops down beside Leon. The handgun barely hurts him: his health is the game's 20000 (200 handgun hits); he flinches each time 1200 of firepower has hit him. **Liquid nitrogen tanks** (the barrel model, read in for this stage) stand on the mission file's kind-06 points (4 listed; 3 of them have floor in the stage data and are placed): shoot one open and the white cloud freezes him within 22 units for 5 s, and each hit then counts 20 times (the port's own; about ten handgun shots). A tank doesn't hurt Leon, and tanks don't set each other off. The stage clears when he and the cultists are down.

**The mission files** (traced in the game's module `bio4_brew.mod`): the 16-byte points (read at 0x45c54) are the stage's *objects*. Each becomes an actor whose kind picks its handler (0x2968c, 0x1b30c) and what a shot does (0x49c48): 01 a hinged door that swings open either way and shuts again (0x1bb00), 02 a ladder (`fix_all` #2; kinds 1 to 6 and 9 all use the part of their own number), 03 an explosive barrel (big blast, 0x4a688 size 3), 04 a breakable object that drops the item its byte 2 names, 05 a canister on chains that falls when shot and blows up (size 5), 06 the liquid nitrogen tank, 07 a treasure (knocked down), 09 something that rings when shot (sound 18; stage 8's notes speak of a bell that makes a loud noise). The enemies are the section at header +8 (read at 0x44b54): 10-byte records of wave, a setting flag (0 always, 1 and 2: the same enemy in two versions, 2 left out here as the harder one), type (0 Leon's start, 1 Ashley there / -1 not, 2 Ganado, 3 cultist, 4 the chainsaw man, 5 Garrador, 6 El Gigante, 7 Verdugo), a variant, a spawn point number (into the stage data's section 3 points, `stg_data_NN`), facing (256 to a turn), two bytes not understood, the drop code (byte 8, the table below; 5 = money is the commonest) and a last flag. **Stages 2-12 follow them** (`ds/source/stage_foes.c`, written by `re4zeebo/gen_stage_foes.ps1` from the files): Leon starts on the type-0 record's point, facing its way; the Ganados / cultists come in wave by wave, the first standing at their points from the start, each later wave once every enemy before it is down (an assumption: the game's own wave trigger was not traced), at most 5 standing at once, and later ones taking the enemy slots of bodies that have gone (there are up to 22 to a stage, 12 slots). Several of a wave on one point come out side by side. The bosses stand on their records' points. The kind-04 objects are crates (break them: the item they name) and the kind-03 ones explosive barrels. Two of stage 7's three stand at height 85 where its map has no floor, beside the four cultists whose records are up there too (the only enemies that high): those four stay at that height and throw burning rocks (the mission's notes: at the word of the men on top, the others hurl burning stones; that they are the throwers is this port's reading), and neither the waves nor the stage clear wait for them. A barrel going off beside them fells them. The game's cannon on that stage (Ashley helps Leon fire it) is not built. Stage 1 keeps its own scheme (its records give 21 enemies in 8 waves, the chainsaw man in the second).

**Doors and bells** (from the mission files' objects): a kind-01 object is a door on its hinge, drawn with the game's door model (`fix_all` #1, its leaf 13 units along its -x): one in stages 2, 5 and 8, three in stage 12, each standing in a doorway of its stage. A at a shut door (A: Open) swings it a quarter turn away from Leon; a Ganado walking into one pushes it open too; it shuts again 4 s later, not while Leon is in its way (the game's door, at 0x1bb00, swings either way and shuts again with a sound; its exact timing was not traced). A kind-09 object is a bell (`fix_all` #9, hanging from its point: two each in stages 8 and 9, in wall alcoves): a shot rings it (the church bell sound) and it swings; the Garradors hear it wherever they are and charge at it (stage 8's notes: "if you strike it, it makes a loud noise"). Stage 12's liquid nitrogen tanks now use the game's own cylinder model (`fix_all` #6) instead of the barrel. The three models are read in only for the stages that have them.

**The hanging canister** (kind 05, stage 8 only): a yellow-labelled canister on three chains (`fix_all` #5), hanging over the long hall at the start, 36 units above the floor. A shot drops it, chains and all (the game pushes the whole model down, harder each frame: 0x3f910), and it blows up on the floor (0x4a688 size 5: 20 units, against the barrel's 60). Here the blast fells Ganados and cultists within 20 units on its level and hurts Leon within 10 by the barrel's 30; the Garradors hear it and charge at it, but are not hurt (their damage from it was not traced). The game's own chain animation is not played.

**Ladders** (kind 02, stages 2 and 6): two in stage 2, three up to the second-floor windows of the stage 6 cabin. The point is the foot of the ladder and its facing has its back to the platform: at all seven points in the files (stage 1's two included) a floor at about 32 lies 16 units behind it, and `stg_data` section 4 holds a landing point nearby. A climbs up or down as on stage 1's ladders. Enemies do not use them. Stage 1 keeps its own two, from action points: one matches the mission file's (319, 68), the other stands at (440, 60) where the file has (440, 214). The follow camera no longer stays up on an upper floor behind Leon after he climbs down.

**Drops** (stages 2-12): the drop code of an enemy record (byte 8) or of a kind-04 object (byte 2) is what it leaves (ff: nothing). The game's module (`bio4_brew.mod`, ARM: the mission loader copies the 16-byte points as they are; the drop table is at 0x49f48, each roll `rand() % n`) reads it as: 1 green herb, 2 first aid spray, 3 hand grenade, 4 flash grenade, 5 600-1000 PTAS, 6 1100-1500 PTAS (steps of 100), 7 handgun / shotgun / rifle / TMP ammo, 8 rifle / TMP / magnum ammo or mine darts, 9 ammo for the weapon in hand, 10 a herb, a spray or nothing, 11 an egg or nothing, 12 and 13 nothing, ammo, a grenade or a herb. The rounds are by gun and count, as the function has them: handgun 10, shotgun 5, rifle 3, TMP 50, Magnum 5 (code 7 one of handgun / shotgun / rifle / TMP, 8 one of rifle / TMP / Magnum / mine darts, 9 the gun in hand's, 12 nothing / handgun / shotgun / rifle / grenade / herb, 13 nothing / TMP / Magnum / darts / flash grenade / herb, rolls `rand() & 3` or `% 6`); 14 and 15 are treasures, the spinel and the blue velvet (2000 and 3000 PTAS, `item_base_info.bin`). Rounds for a gun not in hand go to its own store. The port has no grenades, darts, spray or eggs: grenades and darts leave nothing, the spray and the eggs a herb. Money is picked up like an item ("Pesetas +n PTAS", shown with the pearl icon) and counts towards the results' PTAS. Codes 0 and ff (and past 15) leave nothing, as in the game (the port had given ff a random drop, 70%: gone). Stage 1's Ganados take their records' codes too.

**The elevator** (stage 12): the only story mission against the clock. None of the mission files carries a time limit (the "within the time limit" objectives belong to the Mercenaries-style mini missions, `mis_data_mNN`), but stage 12's notes say the elevator takes "about 4 minutes" to come. Its arrival ends the mission, whether or not the Verdugo is down. The Zeebo video (Verdugo around 2:18:20, Leon in the elevator at 2:20:20) shows no count on screen, so neither does this port: the objective says "The elevator takes about 4 minutes." and "The elevator has arrived." comes up with MISSION COMPLETE. The 4 minutes run while playing, not while paused.

**Collision** (every stage): only the game's own data, `stg_data_NN.bin` section 1. It is u32 groups, u32 triangles, u32, then 16 bytes per group `{ u8 id, u8 walk, u8 camera, u8 shots, u8 kind, 3 bytes, u32 first, u32 count }`, then 36-byte triangles. In `bio4_brew.mod`, 0x64fc tests walkers against the groups with the walk byte set, 0x5728 and 0x71b8 the camera byte, 0x5c80 the shots byte. That those three bytes mean walking, camera and shots rests on this code and on the groups' shapes; which actors carry the flag that selects the camera set was not traced. Leon walks on the walk groups' up-facing triangles and is stopped by their steep ones. The camera tests the camera groups and shots the shots groups, kept in the ground file after the walking data, each piece with a mask. A steep piece is tested as an upright rectangle around it. Groups of kind 1 and 2 have no bytes set and stop nothing (kind 2: door planes; kind 1 is read by the game's shot code). The picture's triangles are no longer used: they were what made invisible walls and walkable roofs.

**Enemies**: every stage has its mission file's enemies again (`-DSTAGE1_ENEMIES_ONLY` takes those of stages 2-12 away). One whose point is where Leon stands (stage 8's first cultist is on his start; stages 2, 5 and 9 send later waves in by the way he came) waits until he is 30 units off it, at his height, or 20 s at most. Stage 7's burning rocks hurt Ashley a third as much as Leon (she ducks; at 30 a rock a few volleys ended the stage).

**Ladders and doors**: the ladders are the mission's kind-02 objects, plus stage 5's, which is part of the chapel's model (its foot and top are stage data section 4, points b3, b4). Its foot is also the mission's event point 3, whose A events (each time: "Leon sets up the ladder", scene 18; with Ashley, once the emblem door is open, "Leon and Ashley climb the ladder", scene 19) had taken the A press, so only the scene played and Leon stayed below. Now at a ladder A plays the event's scene and then the climb, and the prompt reads "A: Climb up" (checked in melonDS: from (14, 49) facing the ladder, the scene, then Leon on top at y 45). The same point's event 13 lets group 4 in (three Ganados, two up on the galleries); with one event a press, the scene's event always came first and group 4 never came. Now on a press the other events of the same carrier that have no scene of their own (only commands) go off too; of all the stages' events only this one is such (the others that share a carrier have lines, as a locked door's beside its key's, and still wait their turn). Checked with `-DTEST_SHOW_FOES`: groups 1 before the press, 17 after, and a Ganado up by Leon within 20 s. Ashley starts at the mission's point for her and waits until Leon comes within 30 units; once Leon has gone up or down a ladder she walks to it and climbs it after him with her own climb (`ch_ash.mcas` #11; down, played backwards). Only where there is no ladder (a drop), or after 8 s without reaching it, she appears beside him. A mission door with a wall right across its doorway (stage 2's, the one Leon came in by) stays locked.

**Weapons**: the handgun (`eqp_wep_all` #1, `ch_leon_w01`), Magnum (#5, `w05`), shotgun (#2, `w02`), rifle (#3, `w03`: the one with the scope) and TMP (#4, `w04`), each with its own rounds, bought in the shop; and the rocket launcher (#6, `w06`; 30000 PTAS, as `item_base_info.bin` has it). The parts were told apart by their pictures (`-Preview`); #6 had been taken for the rifle before. Power and rounds are item_weapon_info.bin's first level. Change with Equip on the status screen, or L+X. Each gun is drawn node by node off the hand (`model_draw_held`), with its muzzle at its barrel-end node. The aim poses are made by the exporter (`6~4@50`: clip 6 turned 50% of the way to clip 4, node by node as quaternions); the rifle's set has no up and down poses, its one aim pose is #3. **The rifle's scope**, as in the Zeebo video: fully aimed, the view is from Leon's eye, 20 degrees across instead of 70, darkened all round a clear round lens with red cross hairs; he isn't drawn, and the shot goes along the view (Up/Down tilt it). **The rocket launcher**: one rocket, flying 5 units a frame to where the sight points; its blast (30 units) fells the Ganados, El Gigante and the Garradors and hurts the Verdugo, then the launcher is gone and the handgun is in hand. The shotgun also hits Ganados in its spread. Not in yet: the guns' own moving parts (`eqp_wep_all.mcas`), their own shot sounds, the scope's zoom.

**Shop**: between the stage select and the mission, as in the game: Start, Buy, Sell and Upgrade tabs with the merchant's lines (`Bio4_StringLayout.bin`, in English). Leon starts with the handgun and 30000 PTAS; the other guns, ammo, Green Herbs and larger cases cost what `item_base_info.bin` says, an ammo box holding what `item_ammo_info.bin` says. Things sell for half (the equipped gun can't be sold). **Vests** (Buy, after the larger case): the Tactical, Iron and Hybrid Vest, one after another, 30000 / 60000 / 90000 PTAS (`item_base_info.bin` ids 0x47-0x49), cut the damage Leon takes by 10 / 20 / 30% (`item_armor_info.bin`; applied as in `GM_CtrlParam::Set_Start`: damage x (100 - cut) / 100; not against the chainsaw's kill). Worn, so they take no room; the icon is the Tactical Vest's for all three. `-DAUTOPILOT=62 -DTEST_SHOP` buys one. Upgrades are `item_tuneup_info.bin`'s (`re4zeebo/gen_shop_tune.ps1` writes `ds/source/shop_tune.c`): firepower, capacity, firing speed, reload speed. Firing and reload speed have no base value in the tables that was identified, so 0.5 and 1.25 x the first level are assumed. Money, rounds, herbs and upgrades carry on between missions and reset on New Game; they are kept in the save file. Different from the game: one press buys one box instead of asking how many; everything is on sale from the start (the game's list grows with the missions, not decoded); grenades, sprays and the rest aren't sold, as the port has no use for them yet; the merchant isn't drawn. Damage is counted in tenths, so weak guns land a point every few hits. `-DTEST_SHOP` starts in the shop with money, `-DAUTOPILOT=34` walks through it, `-DAUTOPILOT=35` comes to it from the title, `-DTEST_ALL_WEAPONS` gives every gun.

**Saving**: `re4ds.sav` (76 bytes; version 2 adds the vest, a 72-byte version 1 file still loads) at the root of the card the ROM runs from, through DLDI: money, guns owned, upgrades, case, vest, rounds, herbs, the weapon in hand. It is written when a mission ends, on leaving the shop and on leaving an empty stage, to `re4ds.tmp` first. With a save on the card the title offers CONTINUE; New Game replaces it at the next save. On a card without a file system nothing is saved. To try it in melonDS, enable DLDI with folder sync and close the emulator normally (the folder is written on closing).

**Polygons**: the DS draws 2048 polygons a frame and silently drops the rest, so what is sent last vanishes: villagers and the laser sight. The stages are cut by the exporter into chunks by place (72 units a cell, `SplitChunks`, flag 2 of the RDS1 file). `model_draw_near()` leaves out what is wholly behind the camera and sends the rest nearest first, one DMA transfer after another from the DMA interrupt, up to a budget. The budget follows what the hardware really kept the frame before (`GFX_POLYGON_RAM_USAGE`, which counts only polygons facing the camera and on screen): it starts with the whole stage, shrinks when a frame reaches 1940 polygons and grows again below 1800, never under 1200 triangles (at 600, as on stage 9's balcony over the hall, the floor and walls near Leon went missing on a real DS). Past that the farthest enemies are left out (nearest first, down to 2), and they come back when there is room; the stage is sent first, so what the hardware drops when a frame is still too full is what comes last, not the walls. (A budget reckoned from the models' triangle counts was far too small, as most of those are culled: whole walls went missing.) `-DTEST_GX` shows the count.

**Carpets and floor decals**: a draw lying on another draw's larger floor (most of its flat triangles within 0.3 above that floor) is raised 0.15 by the exporter (`MarkDecals`), because the DS's depth buffer can't tell surfaces that close apart and showed them in patches (the chapel's carpet). Found: 1 or 2 a stage in stages 5-11, 3 in stage 12.

**Ashley's life**: while she is with Leon, her plate from the HUD sheet (`2d_gms_00`: her name, a life track, the round command icon) is drawn above Leon's, its track coloured as his. The icon shows her following, or the second icon while she waits at her mission point. `-DTEST_ASH_HP=n` starts her hurt.

**Reach check**: `-DTEST_SHOW_POS` shows where Leon stands; `-DAUTOPILOT=30` presses A and walks on (doors, ladders), `-DAUTOPILOT=31` cycles the weapons and fires; `-DLEON_START_X/Z/YAW/Y` set the start.

**Treasures** (every stage): the two kind-07 points of each mission file, up on eaves, ledges and beams, glint with the original's pickup star and the ruby icon (`2d_itemic_00`, added as a row of treasure icons to the items sheet). One within reach of the floor is walked over; one up high is shot down (it falls to the floor below, clear of the walls). Each is worth 3000 PTAS (the video's results show "Joia +3000"; their points' byte 2 is ff, so what the game gives for them is not in the mission files); like any item, it is taken with A within reach; the results screen lists TREASURE found/2.
**Ashley** (stages 5, 6, 7 and 8): the game's model and animations (`ch_ash`, read from NitroFS only for those stages; her skeleton's last 4 bones folded in, to fit the DS's 28). She keeps to Leon's left, a little behind, walking or running to catch up; when Leon aims her way she ducks (shots pass over her). Ganados go for her when she is the nearer; their swings hurt her, and so does a shot that hits her. If she goes down: ASHLEY IS DEAD, game over.

**Stages 2-12** (simple versions, all open in the stage select): each stage's own map and collision (`3d_stgNN_00`, `stg_data_NN.bin`) and haze colour, with Ganados on the game's own enemy points for that stage (`mis_data_sNN.bin`, Ganado kinds only; they all head for Leon). Leon starts on one of those points; the stage clears when all are down. Their bosses, Ashley and the other enemy kinds are not made yet; stage 12's points sit 468, 988 off its model and are moved onto it. The stage models and ground are in NitroFS (`ds/nitrofs`, loaded when a stage is chosen), not in RAM.

Stage 1 is the game's first narrative-mode mission (`mis_data_s01`); its results go back to the stage select. All on-screen text is in English:

1. **The village**: from the fenced path inside the south gate, down every villager (10, at most 5 at once, coming as a group from the open ground ahead). Then, as in the game, **the chainsaw man** (`ch_cha`, with his own walk and chainsaw swing from `ch_cha.mcas`) comes out of a house by the square: 40 health (the handgun barely hurts him; a head shot dazes him for a kick), faster than the villagers, and his chainsaw kills Leon in one blow. The bell rings when he falls or after 90 s.

Each mission starts with its title and objective and ends with MISSION COMPLETE: accuracy, life left, kills, head shots, a total rank and 5000 PTAS. A goes back to the stage select. The village gates stay locked, as in the game ("this door is firmly locked").

Stage 1 gimmicks, placed from the game's own stage data (`stg_data_01.bin`, `mis_data_s01.bin`,
coordinates x256):

- The three village gates (iron gates of the stage model) stay locked.
- **Ladders** up to the walkways (A at the foot climbs up, A at the top edge climbs down); walking
  off a walkway or ledge drops Leon to the ground below. A works anywhere in a strip 6 to either
  side of the ladder's line: below, from 8 before the foot up to the wall under the edge; above,
  from the edge to 14 in (facing within ~65 degrees). (It was a 7-unit circle at the foot and one
  16 in from it, which most ladders' real edge missed: players had to nudge Leon into place.)
- **Ganados round walls**: one whose step is pushed back by a wall for 20 frames (a wall square
  between it and Leon) tries headings 45 / 90 / 135 / 180 degrees off the line to him, its last
  side first, each probed 8 units against the walls and drops, and walks the first clear one for
  70 frames (`enemy.c`, `pick_detour()`). Before, it trod on the spot by the wall, yet still hit him
  when he came close.
- **Crates** (kick with A, or shoot), at the stage's action points a10/a11: may hold ammo or a herb.
- As in the Zeebo version's stage 1 (checked against a full playthrough video), there are no
  explosive barrels and no blue medallions. The code for them stays in `objects.c` for later stages.
- **Ganado weapons** (as in the Zeebo version's village): each carries what its mission record's kind says (a hatchet or a pitchfork, `eqp_wep_all` #10, #11, in the right hand; bare hands; see **Where a blow lands**) and walks and swings with the game's weapon animations (`ch_gna_w00`: one-handed, `ch_gna_w01`: pitchfork, which reaches further). A blow costs 32 health; the bare-handed grab.
- **Kick** ("Chutar"): a head shot that doesn't fell a Ganado leaves it dazed for 1.5 s; walk up and press **A** (not aiming, `A：蹴る` shows) for Leon's kick, which knocks it and any Ganado beside it back (2 damage).
- **The Ganados** of the stage (10, at the game's spawn points: on the ground, on walkways, on a
  rooftop), at most 5 in the stage at once; the next comes in as one falls, one out of a house
  door. They drop off ledges to reach Leon. The stage clears when all 10 are down.
- Without the gun raised, the top screen shows what A does here (`A：はしごを登る` etc.).

Personal research project. `ds/data/` and the built `.nds` contain Capcom's game data:
don't distribute them.

## Controls

Menus: Up/Down (and Left/Right in the stage select) choose, A (or START) confirms, B goes back
(and resumes from the pause menu's list, as START does).

### Control Leon (the game starts in this mode)

| Button | Action |
|---|---|
| D-pad Up / Down | walk forward / walk backward |
| D-pad Left / Right | turn |
| B + Up | run |
| X | draw the handgun into the ready stance / put it back in the holster |
| R (hold) | aim the handgun two-handed (laser sight) — Up/Down tilt the aim (about ±50°), Left/Right turn slowly |
| A (while aiming) | fire (10 rounds per magazine) |
| L (hold) + A | knife: the view moves in over his shoulder as when aiming (Left/Right turn, Up/Down tilt the view); A slashes: low (breaks a crate at his feet, or cuts a Ganado, 1 damage), or at chest height when tilted up |
| A (not aiming) | kick a dazed Ganado / kick a crate / climb a ladder up or down (the action is shown on screen) |
| Y | reload (also automatic when firing on an empty magazine) |
| Touch (bottom screen) | vital screen: tap an item slot to select it, double-tap the herbs to use one |
| A / B / X / Y (mash, when grabbed) | break free |
| START | pause menu |
| SELECT | the status screen (the weapon list) |
| L + SELECT | switch to the free camera |
| Y + SELECT | polygon culling: back / front / off |

### Free camera

| Button | Action |
|---|---|
| D-pad | move forward/back, turn |
| L / R | strafe |
| X / B | look up / down |
| Y / A | move down / up |
| Touch + drag (anywhere) | look around |
| START | Leon demo: idle / walk / run around a circle / hidden |
| SELECT | back to controlling Leon |

If loading fails, the bottom screen switches to text and shows which file.

## Running it

The ROM is `ds/re4ds.nds` (DS and DSi; it uses no files, so no DLDI patching is needed).

- **DS / DS Lite with a flashcart:** copy `re4ds.nds` to the SD card and start it from the
  flashcart menu.
- **DSi / 3DS with TWiLight Menu++:** copy it to `sd:/roms/nds/` (or anywhere TWiLight lists)
  and start it. On a DSi it runs in DSi mode, on a 3DS through nds-bootstrap.
- **Emulator:** `tools/melonDS/melonDS.exe ds/re4ds.nds`, or `./build.ps1 -Run`. Keyboard
  mapping is under melonDS *Config → Input and hotkeys*; the mouse is the touch screen.

## Building

Needs Docker (BlocksDS image `skylyrac/blocksds:slim-v1.24.0`) and PowerShell 7.

1. Convert the game data (only after changing what's exported):

   ```
   ./re4zeebo/re4zeebo.ps1 -InputPath "<Resident Evil 4 - Zeebo Edition.zip>" -OutDir ds/data `
       -DsModel 'stage=3d_stg01/stg_01.mcms', 'leon=3d_base/ch_leon.mcms', 'gun=3d_base/eqp_wep_all.mcms#1', 'ganado=3d_base/ch_gna.mcms' `
       -DsAnim 'leon_anim=3d_base/ch_leon_base.mcas#22,23,24,11,20,21,13,3,10,8,16,0,1,2', 'leon_gun=3d_base/ch_leon_w01.mcas#0,1,2,8,9,4,6,5,7,3', 'ganado_anim=3d_base/ch_gna_base.mcas#6,7,12,21,9,8,10' `
       -DsGround 'stage_ground=stage+3d_stg01/stg_data_01.bin' `
       -Ds2D 'title=file:assets/title/title_ds_edition_source.png@0,0,1109,832>256,192', 'hud=common/2d_gms_00.mct@128,0,128,128', `
             'stagesel=common/2d_stage_00.mct@0,0,256,224', 'result=common/2d_gm_00.mct@0,48,256,192', `
             'items=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24&common/2d_itemic_00.mct@96,96,32,24!cut32x24|common/2d_itemic_00.mct@128,0,128,24!cut32x24', 'star=3d_base/fix_all.mcts#0@0,0,64,64!glow', `
             'beam=3d_base/fix_all.mcts#7@0,0,32,64!glow', 'spark=3d_base/eff_all.mcts#0@0,64,64,64!glow', `
             'vitalbmp=common/2d_bks_01.mct@0,0,240,96|common/2d_bks_00.mct@192,0,64,84|common/2d_bks_01.mct@0,128,240,32!bmp', 'iconbmp=common/2d_itemic_00.mct@0,0,128,72!cut32x24!bmp' `
       -DsFont 'font=common/2d_fnt_00.mct;common/2d_fnt.hfi;ds/source/text.h' `
       -DsSound 'sfx=common/snd:wep15_shot5,fn57_kara,wep15_reload_re,bogan_kabe,v_damege03,v_fd1,ikaku01,kazekiri,zea_c_down,leon_dam1,leon_dam2,escape_hit,sub_kettei2,itemget_fixeq2,demo_kane,door_w01_kick,tate_break1,handbomb,knife' `
       -DsMusic 'music=common/snd:st001,st003'
   ```

   Keep the quotes: unquoted, PowerShell splits `#0,1,2` into separate arguments.
   `-Ds2D` crops (and resizes) a texture into a picture sheet (`#n` picks a texture of an
   `.mcts` set; `!glow` makes light-on-black effect textures translucent, brightness becoming
   alpha; `!cutWxH` makes the black around icons in W x H cells transparent; `!bmp` writes a 16-bit bitmap for the bottom screen instead; several sources joined by `|` are stacked; `file:path.png` takes an image file instead of a game texture); `-DsFont` keeps the glyphs of
   the game font that `text.h` uses, and lists any it lacks (the font has about 1000 kanji
   and no ポ/ボ, for instance). After changing menu text, re-running just `-DsFont` is enough.
   `-DsSound` decodes the listed WAVs into one bank; the order must match the `SFX_*` list in
   `ds/source/sfx.h`. `-DsMusic` plays the MIDI tracks in real time through Windows' MIDI
   synth and records the PC's audio output (WASAPI loopback), so nothing else may play while
   it runs (it takes as long as the tracks, about 1.5 minutes); the recordings are also saved
   as `re4zeebo/out/music/*.wav`. Track order must match `MUSIC_*` in `ds/source/music.h`.

2. Build: `./build.ps1` (or `./build.ps1 -Run` to start melonDS afterwards).

### Test builds

`docker run --rm -v "${PWD}/ds:/work" -w /work skylyrac/blocksds:slim-v1.24.0 make -B "DEFINES=..."`

| DEFINES | What it does |
|---|---|
| `-DTEST_SHOW_FOES` | FPS field: foes tried x 100 + moved near Leon; herbs: foes standing; magazine: the nearest one's distance |
| `-DTEST_FOE_DUMP` | after the stage loads, lists every enemy slot's place on the bottom screen and stops |
| `-DAUTOPILOT=51` | from the title through the stage select and the shop into stage 6 (the battlements), the opening skipped, then turning on the spot |
| `-DAUTOPILOT=50 -DTEST_HIGH_CRATE -DTEST_SHOW_TILT` | the knife: look up and slash (a crate placed 13 up in front of Leon breaks), then look down and slash (FPS field: the tilt in percent) |
| `-DAUTOPILOT=49 -DTEST_INF_AMMO` | aim, a few shots, then Y (reload) and Up: Leon walks while reloading |
| `-DAUTOPILOT=48` | from the title: New Game, then Right / Left on the stage select |
| `-DTEST_CUTIN` | the stage's opening scene plays (with `-DCUT_AUTO=60`: a page a second) |
| `-DAUTOPILOT=1` | scripted walk / turn / run / walk backward |
| `-DAUTOPILOT=3 -DLEON_START_X=225 -DLEON_START_Z=204 -DLEON_START_YAW=106` | runs head-on into a barn wall |
| `-DAUTOPILOT=4` | aim, tilt up/down |
| `-DAUTOPILOT=5` | draw, walk and run with the gun, holster |
| `-DAUTOPILOT=7` (+ the barn-wall start above) | aim and fire every 0.4 s, reloading when empty |
| `-DAUTOPILOT=8` | let the Ganados walk up, then shoot them |
| `-DAUTOPILOT=8 -DTEST_GOD -DTEST_INF_AMMO -DTEST_SHOW_HP` | the same, with the last one shot's health in the FPS field, shots that hit in the spare-rounds field, staggers in the herbs, felled in the magazine |
| `-DAUTOPILOT=9` | walk into the Ganados and stand there, tapping A (attacks, breaking free, death) |
| `-DAUTOPILOT=10` | menus: title, controls screen, back, stage select (a locked stage, then the village), pause, start over |
| `-DAUTOPILOT=21 -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=2` | the pause menu: Quit (the question, YES, back), Sound (Medium), Controls, back to the game |
| `-DAUTOPILOT=22 -DTEST_HURT -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=2` | the status screen: the handgun's and the ammo's commands, Use on a herb, scrolling down, back to the game |
| `-DTEST_HANG_AT=n` (`-DTEST_HANG_RAY`) | at frame n the hanging canister in stage 8 drops (through the shots' own hit test, from Leon's gun height) |
| `-DAUTOPILOT=23 -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=8 -DLEON_START_X=138 -DLEON_START_Z=217 -DLEON_START_YAW=90` | Leon aims up and fires in turn until the canister falls |
| `-DAUTOPILOT=12 -DTEST_HURT -DTEST_GOD` | vital screen by scripted taps: select slots, a slow second tap (nothing), double taps on the herbs (use, use, health full) and on the ammo (nothing) |
| `-DTEST_HURT` | start with 30 HP and 3 herbs |
| `-DAUTOPILOT=11 -DTEST_GOD -DTEST_KILL_AT=600` | every Ganado goes down at frame 600 leaving items; Leon walks over them, the stage clears, results, stage select |
| `-DSKIP_TITLE -DTEST_DROPS -DTEST_KILL_AT=2 -DTEST_NO_CLEAR` | an ammo box and a herb in front of Leon (and the Ganados' items) to look at |
| `-DTEST_DROP_CODE=n` | every enemy on a mission point leaves drop code n's item, and one is dropped 20 ahead of Leon at 6.7 s (with `-DAUTOPILOT=11 -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=2` he walks over it) |
| `-DTEST_KILL_EVERY=n -DTEST_SHOW_FOES` | every n frames the enemies in the stage go down; the FPS field shows enemies brought in x100 + wave x10 + standing (the spare-ammo field: how many in all) |
| `-DTEST_GOD` | the Ganados never hurt Leon |
| `-DAUTOPILOT=13 -DSTAGE_START -DLEON_START_X=240 -DLEON_START_Z=565 -DLEON_START_YAW=180` | kick open the south gate, walk in |
| `-DAUTOPILOT=14 -DSTAGE_START -DLEON_START_X=310 -DLEON_START_Z=76 -DLEON_START_YAW=90` | ladder up and down |
| `-DAUTOPILOT=60 -DTEST_SHOW_POS -DSTAGE_START -DLEON_START_X=298 -DLEON_START_Z=76 -DLEON_START_YAW=90` | ladder as a player takes it: A from 10 past the foot (up), a few steps to the edge, A (down) |
| `-DSKIP_TITLE -DSTAGE_START -DTEST_GOD -DTEST_SHOW_FOES -DTEST_SIGHT=250 -DLEON_START_X=492 -DLEON_START_Z=400 -DLEON_START_YAW=270` | Leon east of the L-shaped house, the Ganados west of it: they walk round it to him (nearest one's distance in the magazine field; with `-DTEST_NO_DETOUR` they stay pressed against its walls, as before) |
| `-DAUTOPILOT=15 -DSTAGE_START -DLEON_START_X=340 -DLEON_START_Z=76 -DLEON_START_YAW=270` | walk off a walkway and drop |
| `-DTEST_BOSS -DLEON_START_X=440 -DLEON_START_Z=262 -DLEON_START_YAW=180` | the chainsaw man comes at once, right in front |
| `-DAUTOPILOT=18 -DSTAGE_START -DLEON_START_X=391 -DLEON_START_Z=322 -DLEON_START_YAW=90` | knife out next to a crate, slash it |
| `-DTEST_KNIFE_VIEW` | with L held, Leon plays each knife clip in turn (number in the FPS field) |
| `-DSKIP_TITLE -DSTART_STAGE=n` | straight into stage n (1-12) |
| `-DTEST_PROF` | the vital screen shows frame timings (0.1 ms, peak over a second): spare rounds = update, herbs = waiting for the stage, FPS = drawing the characters, magazine = waiting for the next vblank |
| `-DAUTOPILOT=19 -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=4 -DTEST_GOD -DTEST_KILL_AT=20 -DTEST_AUTO_CUT -DTEST_SHOW_GIANT -DTEST_INF_AMMO` | the giant fight played out: shoot straight ahead, cuts automatic; the FPS field shows shots left x100 + state x10 + cuts |
| `-DTEST_SHOW_THROW` | the FPS field shows throws so far x100 + the first thrower's state x10 + missiles in flight |
| `-DGIANT_AHEAD_TEST -DGIANT_AHEAD=n -DGIANT2_DEG=d` | (instead of their points) the first giant starts n units ahead, the second d degrees to the left (stage 11); `-DTEST_SHOW_GIANT` shows the second's state in the spare rounds field |
| `-DTEST_ELG_VIEW` | the giant plays each of his clips in turn |
| `-DTEST_GAL_VIEW -DGAL_AHEAD=10 -DCAM_TEST_SIDE` | the Garrador plays each of his clips in turn, beside Leon (clip number in the FPS field) |
| `-DAUTOPILOT=19 -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=8 -DGAL_AHEAD=30 -DTEST_GAL_BACK -DTEST_SHOW_GAL -DTEST_GOD -DTEST_INF_AMMO` | the Garrador starts with his back to Leon, who shoots straight ahead; the FPS field shows his health x10 + state |
| `-DTEST_VER_VIEW -DVER_AHEAD_TEST -DVER_AHEAD=12 -DCAM_TEST_SIDE -DSKIP_TITLE -DSTART_STAGE=12` | the Verdugo plays each of his clips in turn, beside Leon (clip number in the FPS field) |
| `-DAUTOPILOT=19 -DSTAGE_START -DSKIP_TITLE -DSTART_STAGE=12 -DVER_AHEAD_TEST -DVER_AHEAD=30 -DTEST_TANK_AT=330 -DTEST_SHOW_VER -DTEST_SHOW_TANKS -DTEST_GOD -DTEST_INF_AMMO` | the tank nearest the Verdugo bursts at frame 330 while Leon shoots straight ahead; the FPS field shows tanks standing x100 + his state x10 + health / 12 (`-DTEST_SHOW_VER` alone: health x10 + state) |
| `-DLEAP_EVERY=60` | the Verdugo leaps up after 1 s of stalking (default 9 s) |
| `-DELEVATOR_FRAMES=900` (`-DOBJ_FRAMES=1200`) | the stage 12 elevator comes after 15 s (the objective stays up 20 s) |
| `-DTEST_KNOCK_AT=n` (`-DTEST_KNOCK_AHEAD=u`) | at frame n the treasures up high come down (landing u units ahead of Leon) |
| `-DTEST_SHOW_DROP` | the FPS field shows the first treasure's distance from Leon x10 + 1 if up high |
| `-DTEST_SHOW_TANKS` (with `-DTEST_SHOW_VER`) | the FPS field shows tanks standing x100 + the Verdugo's state x10 + health / 12 |
| `-DTEST_ASH_VIEW` | Ashley plays each of her clips in turn in front of Leon (clip number in the FPS field) |
| `-DTEST_WEAPON=1` (2, 3) | every Ganado carries a sickle (hatchet, pitchfork) |
| `-DSTAGE_START` | autopilot builds start at the game's start point, gates shut (else in the square, gates open) |
| `-DSKIP_TITLE` | start in the game (the other autopilots do this too) |
| `-DAUTOPILOT=9 -DTEST_MASH -DTEST_SHOW_REACT -DTEST_WEAPON=0 -DLUNGE_PERCENT=100` | bare-handed Ganados grab Leon; A is mashed for him: the struggle prompt, the break-free kick, then `A: Kick` (FPS field: reaction x 100 + grabs so far) |
| `-DLUNGE_PERCENT=100` / `=0` | Ganados only lunge (grab) / only swipe (default 40% lunges) |
| `-DCAM_TEST_SIDE` | camera looks at Leon from his right side |
| `-DSTART_CTRL=1` | start in the free camera |

Rebuild without `DEFINES` afterwards for the normal ROM.

## Layout

- `re4zeebo/` — PC tool (C#, run through PowerShell 7): unpacks the Zeebo archives, converts
  textures/models (PNG, OBJ) and exports DS data (`DsExport.cs`, 2D sheets and font: `Ds2D.cs`,
  sounds: `DsSound.cs`, music: `DsMusic.cs`).
- `ds/source/` — the DS program: `model.c` (models, skinning on the matrix stack),
  `anim.c` (keyframes, blending), `ground.c` (floor height, walls), `vital.c` (bottom screen: vital / item screen),
  `enemy.c` (Ganados), `sheet.c` (2D pictures as screen-space quads: HUD, title),
  `text.c` / `text.h` (game-font text as sprites, menu strings), `sfx.c` (sound effects), `music.c` (background music), `drops.c` (item drops), `objects.c` (stage gimmicks),
  `main.c` (screens, game
  loop, controls, camera, fog).
- `tools/melonDS/` — emulator used for testing.

**Spawn flow (as libbio4af.so's GM_CtrlMain_Enemy / GM_CtrlMain_Event)**: 5 enemy units (Init(5, 50)). A free unit takes the first record in file order whose group has been let in, that has entries left (EntryMax, record byte 7) and none in the stage, and puts it on its own point. A unit is freed once the body has gone. Group 0 is let in at the start. The other groups are let in by the mission's events: section +16 holds 28-byte records (category, trg, condition, value, scripts). The scripts at +20 use command 210 to let a group in.
- Conditions: kills, time (minutes), a record down, an event played, a flag, an item held, and an event wall touched (collision groups of kind 2, in order).
- Triggers can also hang on an enemy record, a broken object, or an event point (stg_data section 4, within the event's reach).
- First AI (byte 6): 6 and 7 go for Leon at once; the others wait to notice him.
- Removed: the old relocation of far foes near Leon and its alert.
- Clear: every group let in is down and no group still comes by itself (kills/time). Groups waiting on Leon going somewhere don't hold it back. The game's own clear is mission flag 0 (mostly reaching an exit point); this is not done yet.

**Mission goals (the events, continued)**: each event also says how Leon sets it off: at +20 (GM_CtrlMain_Event::_Check_EveGetCond): 0 by itself, 1 by touching it, 2 with the action button ("A: Examine" at an event point, "A: Pick up" at a broken crate or a fallen enemy that carries it), and 3 the button with Ashley by him. +21 says whether an event-point one plays each time (a locked door's line); others play once.
- **Scenes**: the scripts' pages (texts, pictures) play as scenes. gen_cutins.ps1 now exports every script of stages 2-12; the in-play lines are translated in cutins_en.txt (the "Event messages" block). Stage 5's Ashley scene and stage 8's Garrador scene keep the port's own triggers. When they play, the events that carry them count as played.
- **Items**: these come from item_base_info.bin (id, category, price):
  - keys and the emblem (80-86) are held;
  - the emblem's halves (80 + 81) become the Hexagonal Emblem (82) by themselves;
  - treasures (100-121) pay their price;
  - herbs (40-42) are green herbs here;
  - files (200-212) are a line on the bottom screen.
- **Clear**: a stage whose events set mission flag 0 clears when it is set: stage 2 at the valley door with the emblem, stage 3 at the waterfall's end, stages 5/7/8/9/10 at their way out (most with Ashley), stage 6 after 25 kills. Stages 4, 11 and 12 keep their own ends (the giants, the elevator). Stage 1 also keeps its own.
- **Tested in melonDS**: stage 3's way out (A: Examine, then MISSION COMPLETE and its clear scene); stage 2's crate 2 (A: Break, A: Pick up, "Got the Emblem (Right Half).", group 2 let in).
- **Not tested**: the Ashley stages' ways out; stage 5's key from the fallen Ganado.

**Leon's speeds (the game's, scaled)**: ch_param_0's motion scripts, through GM_Ctrl_ObjScript::_Cmd_Move: the script's value, plus MoveSpd when its third value is 0. MoveSpd is set from the character's kind by GM_CtrlSub_ChHandler::Entry → Set_DefParam; Leon's kind has MoveSpd 300 and RotSpd 900.
- **Moves per tick**: walk (motion 2: clip #23) 300; run (3: #24) 700; walking back (4: #23 reversed) 400. Hurt (under 800 of 1200): 280, 600, 380.
- **Quick turn** (7: #23): half a turn in 6 ticks, with 200 a tick ahead from the 2nd tick.
- **Android stick** (GM_CtrlMain_Player::_State_Move; the virtual pad's rates are 0-100%): it runs at a rate of 71% or more, and turns at RotSpd × the sideways rate / 100.
- **Scale**: at the 20 ticks a second assumed elsewhere, everything would be 1.45 times what the Zeebo video shows, with the feet sliding. So the port keeps the measured walk (0.27 a 60 Hz frame) and sets the others by the game's ratios (`LEON_MOVE(s)` = 0.27 × s / 300): run 0.63, back 0.36 (measured in melonDS: 43 units in 2 s), hurt 0.25 / 0.54 / 0.34. Walking back plays the walk 4/3×; the other clips play at their own 20 frames a second. Measured on the planted foot (ankle within 0.25 of its lowest, from leon.bin and the clips): walk #23 ~15.6 units/s against the move's 16.2, run #24 ~37 / 37.8, hurt walk #1 ~14 / 15.1, hurt run #2 ~31 / 32.4, handgun walk ~15, run ~35. The run had played 1.575×, its feet going back ~58 a second against 37.8: sliding.
- **Turning** stays at the video's 240 degrees a second: 900 a tick would be 68-99 degrees a second, against the video.
- **Quick turn**: B with Down, 26 frames, moving from its 9th frame. Only the two pressed together (within 6 frames, QTURN_PAIR): B pressed while walking back had turned him (2026-11-11), now he keeps backing. `-DAUTOPILOT=71` checks both (melonDS, stage 5: B held while backing, no turn; Down and B together, the turn).

**Weapons (item_weapon_info.bin)**: "WPI", u16 16 weapons, u16 7 levels, then 30-byte records of u16s, per weapon and level (65535: as the level before). The fields, from Bio4_WeaponNode's getters:

| Offset | Field |
| --- | --- |
| +0 / +2 | near / far power |
| +4 | hitback |
| +6 | far distance |
| +8 | shot distance |
| +10 | magazine |
| +12 | case size |
| +14 | ReactionSpeed |
| +16 | ReloadSpeed level |
| +18 | ThroughRate |
| +20 | rounds a shot (shotgun 5) |
| +22 | scope |
| +24 | critical |
| +28 | bullet id |

Weapons 4-9 are the handgun, Magnum, shotgun, rifle, TMP and launcher.
- **Refire**: ReactionSpeed, in ticks. Handgun 10/8/6 by its firing-speed level: 0.5, 0.4, 0.3 s at 20 a second, as the shop's table says. That settles 20 ticks a second for the guns. Magnum 14, shotgun 30, rifle 40, TMP 2, launcher 47. The port's handgun had fired every 3. Checked in melonDS: A pressed 20 times in 2 s fires 5 shots.
- **Reload**: ch_param_0's reload motions (type 16) are picked by the ReloadSpeed level (motion condition 6). Their clip is #36, and command 12 ends the reload at frame:
  - handgun 18/15/11
  - Magnum 62/49/37/31
  - shotgun 44/35/18/10
  - rifle 35/28/18
  - TMP 32/28/23/18/14

  The port plays the gun's whole reload clip in that many frames.
- **Through** (+18 → GM_Obj_Eqp_Weapon::Set_TrgHitMax): how many more one shot may hit. Magnum 2, the others 0 at their first level. The Magnum's shot goes on to up to 2 more behind the first, within 3 units of its line.
- **Not done**: the shotgun's rounds. Each of 5 (7, 9, 12) does near 120 / far 80; their directions come from nodes of the gun's model (Get_ShotGunVec). The port still hits all in a cone with one shot's power.

**In-play messages**: in the Android game, an event's lines without a picture, and an item taken ("'Playing Manual 1' taken"), show in a box over the play view, with the game standing still (checked in its Android longplay video). The port does the same with MODE_MESSAGE: the last 3D frame dimmed, like the pause screen, who speaks in gold, up to three lines, and A or B for the next page. Scenes with a picture still play on their own screen (MODE_CUTIN). Keys and files taken show as "'<name>' taken."; treasures and herbs keep the port's popup.
**Objectives**: each mission's start now names its own goal, worded from what its events need for flag 0 (text.h T_OBJ_2..11; stage 12 keeps the elevator's line). They are the port's wording: the Android video shows no objective banner at a mission's start.

**Rank and the merchant's discount** (Bio4_CTTask_GameResult::_calcResult_MS):
- **Score**: hit rate + life + attack, each 0-100.
  - Hit rate: hits × 100 / shots.
  - Life: 100 - (damage to Leon and Ashley) / 100, in the game's 1200 health. Here 12 × the port's damage, Leon's only.
  - Attack: enemies felled against all the mission's (GetMSAttackScore), weighted 1, 1, 10, 50, 100 by class. Here 1 a Ganado or cultist, 10 the chainsaw man, 50 a Garrador, 100 El Gigante or the Verdugo.
- **Rank** (GetMSRank): over 149, 179, 209, 269 → ranks 1-4. The letters D, C, B, A, S are the port's; the Android video skips its results screen.
- **Discount**: the next shop's buy prices take rank_discount[last rank] percent (100, 100, 100, 90, 80; Bio4_ItemNode::GetBuyPrice; upgrades unchanged). At rank 3 or 4 the merchant's opening lines become "You're quite a shot. / I'll give you a discount." The Android video after mission 1 shows the shotgun at 18000 instead of 20000. The last rank is not saved.
- **Shop stock by mission** (Bio4_ShopItemList::_isArrival): a mission clear flag per item, read from the Android item table, which is encrypted. The Zeebo item_base_info.bin has no such field, so the port's shop keeps everything on sale.
- **HUD**: the port keeps the Zeebo layout; the Android one, top right, is not taken.

**Fog by stage**: each mission file sets its own fog (GM_Data_MisParam: FogCol, the haze colour at +9..11 of section 1's record; FogDis 0 / 1, the distances it goes from and to, at +60 / +64, in 1/256 units). The camera sets a linear fog from those (GM_CtrlMain_Camera::Set_Fog, type 1). By stage (from–to, world units):

| Stage | Fog | Stage | Fog |
| --- | --- | --- | --- |
| 1 | 10–781 | 7 | 10–796 |
| 2 | 10–484 | 8 | 125–1097 |
| 3 | 10–625 | 9 | 129–940 |
| 4 | 10–984 | 10 | 323–1097 |
| 5 | 125–273 | 11 | 0–508 |
| 6 | 5–413 | 12 | 10–786 |

The values live in stage_info[].fog, and stage_fog() fills the hardware fog table:
- The density is linear over that range, cut at the far plane (700).
- The table's step (glFogShift) is the finest that spans the range in 32 entries: depth bunches up far off, and a range starting at 323 had covered the whole screen with the 128-unit step.
- The port's earlier fog (exponential from 15, the same everywhere) is kept for comparison behind `-DTEST_OLD_FOG`.

**Shotgun rounds**: 5 a shot, each from Leon's chest along the aim, 4-5 degrees apart about it.
- The aim is the sighted enemy's direction if the sight is on one, else his facing, tilted as he aims.
- Each round hits the nearest Ganado or cultist it passes within 3 units of, up to SHOTGUN_RANGE, for 120 within 6 m (60 units), else 80.
- Checked in melonDS: with the sight off-centre, 1 of 5 hit at 12 units.

**Life score**: Ashley's damage now counts too, as 6 × the port's, her 600 against the port's 100.

**Ways out with Ashley**: stage 5's was checked in melonDS with test hooks (`-DTEST_ASH_WITH -DTEST_EV_PLAYED=8`, Leon started at its point). A by the door with Ashley by him gives "After a long struggle...", then MISSION COMPLETE and the clear scene. The walk there was not tested.

**Treasures' worth**: what a treasure (object of kind 07) gives is the item of the mission event hanging on it (category 4 on that object; its script's command 203). gen_stage_foes.ps1 writes stage_treasure_item; the port pays that item's price from item_base_info.bin when it is taken (stage 2: Antique Pipe 10000 and Spinel 2000) instead of a flat 3000.
**No extra safe time**: Leon's reaction motions (ch_param_0 types 27-39, 53) start with script command 118 (GM_Ctrl_ObjScriptCh::_Cmd_Ch_SetMuteki). He is untouchable through the reaction and no longer, so SAFE_FRAMES is now 0 (it was 90).
**Alerts**: only a leader tells the others. Leaders are marked in the enemy kind (+22, GM_Data_ChP_Info::Get_Idv_LeaderFlg): Ganado kinds 6-8 and cultist kinds 5-8. A leader with a target points (GM_Hdl_Ch_Normal::_Check_Yubisashi, motion 53, not drawn here), and every enemy in the stage takes Leon for its target (Set_AllTrgFind). Others notice only by their own sight and hearing, or by being shot. The port's earlier spread to all within 80 units is gone. Checked in melonDS: running in stage 1, all 5 villagers came.
**Save version 3**: the last rank is saved too, so the merchant's discount survives a restart. Version 1 and 2 files still load, without a rank.

**Camera (the game's)**: GM_Task_Mis_Prepare::_Main_MisCam → GM_Hdl_Cam_Behind::Set_DefCam takes the mission file's section 1 record:
- DefCamPos +32..40 (s32/256): the point it turns about, from Leon: 2.6 right, 2.6 behind, 15.9 up (16.7 in the giants' stages 4 and 11).
- DefCamRot +44..52 (65536 a turn): 600 = 3.3 degrees down (-200, looking up, in stages 4 and 11; 2400 in stage 10), and 1200 = 6.6 degrees round to the right.
- DefCamDis +56: 19.5 units from that point.
- The view is 45 degrees (the literal next to it).

cam_setup() turns these into the port's eye and look-at offsets for each stage (about 22 behind, 4.8 right, 17 up in most) with a 45-degree view. The port had used its own: 14.5 behind, 1.5 right, 16.5 up, a 70-degree view. The aiming camera keeps the port's offsets. Checked in melonDS (stages 1 and 4).

**Enemies on ladders**: the game's AI takes ladders (GM_CtrlSub_ChAi::Check_Ladder); the port's enemies had not. Now one that has Leon for its target, with him on another floor (10 units or more apart), walks to the nearest ladder (OBJ_LADDER, within 120) whose foot (going up) or top (going down, 16 behind the foot at the platform) is on its floor, and climbs it in 75 frames. That time is the port's own: the Ganados have no climbing clip here and rise in their walk. Checked in melonDS: with Leon on the stage 6 cabin's upper floor, the Ganados outside came up the ladders through the windows and grabbed him within 30 s.

**Blasts (the game's)**: GM_Hdl_Fix_Normal::_Set_Exp calls GM_CtrlMain_ObjBox::Set_AllRangeAtk twice per blast, with the radii as s32/256.
- A barrel (size 3): out to 30 and 60 units, power 1600 near and far, more than any Ganado's or cultist's health. BLAST_RANGE is now 60 (it was 24): Ganados within it fall, and Leon is hurt within the inner 30 (the port's BLAST_DAMAGE, 30).
- A hanging canister (size 5): 10 / 20 units, power 1200 / 1000. HANG_BLAST was already 20 (10 for Leon).
- Not tested in melonDS.

**Not changed, the data unsettled**:
- Throwers: the cultists' throw scripts use command 115 (GM_Ctrl_ObjScriptCh::_Cmd_Ch_ThrowBull): bullet 8 of item_bullet_info.bin (13-byte records: power 1600 / 1600, then 350), with angles 0-50 and 160-235. How that power comes to Leon's 1200 health isn't settled, so MISSILE_DAMAGE stays 20 of 100.
- Ashley's following (GM_CtrlSub_ChAi::Main_Npc): only a "close to the leader" test at 2000 (7.8 units) was found. The port's follow distances (stops within 3, runs beyond 28) stay.

**Fixes after the camera change**:
- **Status screen**: its Leon is placed for a 70-degree view. With the play view at 45 he came out large and off his place, so the screen now sets its own (ST_FOV 70) and the play view's comes back on leaving it.
- **Backing and turning**: the follow camera had eased its eye and look-at points across the ground, 1/8 a frame each. Turning, they cut inside the arc, more the farther back the camera sits, and walking back (now 0.36) and turning swung the view about. Now its heading (cam_yaw) eases after Leon's (1/8 a frame, 1/2 aiming), and the eye and look-at sit at their set distance from him every frame; only their height still eases.
- **Fences**: with the camera farther back (22), it had more often ended up behind thin fences (stage 1's), seeing Leon through the posts: the camera's ray test only knows the collision groups flagged for the camera. Now the walls that stop Leon stop it too. From his head out along the line, every CAM_WALL_STEP (1.5 units), a probe 0.75 wide between his chest and the camera's height is tested; the first one inside a wall is as far as the camera goes, and pressed close, he fades as before. Checked in melonDS, backing into stage 1's fences.

**Results time digits**: in 2d_gm_00 the digits 2-9 sit side by side, 8 wide from x 64 (0 at 48, 1 at 60, 4 wide). The port had read 7-9 four pixels too far right, so any time with a 7, 8 or 9 showed broken digits. Checked with `-DTEST_RESULT_SECONDS` (78:59, 17:57).

**Stage 8's key**: the Prison Key (84) is taken by examining the picture frame at event point 3 (378, 59, 137), which stands 19 over its floor (40). The event points' vertical reach was -16..+8 from Leon's feet, so "A: Examine" never came up there, and the door at point 1 only said it was locked. The reach is now -24..+8. An audit of every stage's points against the floor under them finds all within it; floors are 30 or more apart. Checked in melonDS: "Prison Key" taken at the frame, then "You used the dungeon key to open the door.", then A: Open.
**Stage 7's bridge to the cannon** (y 40, x 208-308, z 324-347): its floor and railings are whole in ground07 (checked by the ground's cells), and walking it straight, or at an angle into either railing, keeps Leon on it. What could carry him off is a push: the Ganados and Ashley push him before the walls do, and a crowd on the narrow bridge could shove him right across a thin railing, the walls then pushing him out on its far side. Now, if a move would drop him and his own step alone would not, he takes only his own step. The fall itself was not reproduced, so the cause is not confirmed.

**Can every stage be cleared? (2026-11-11)**
- *Reach audit* (`-DTEST_AUDIT -DSKIP_TITLE -DSTAGE_START -DSTART_STAGE=n`, audit_stage()): from Leon's start, a flood fill over the floor in 2-unit steps with his own collision (STEP_UP 3, radius 2.5, height 16), drops off ledges, the ladders both ways and stage 5's chandelier jump; doors taken as opened (a mission door opens with A, key or not). It prints, for every event point that has events (within their reach and height window), every crate and treasure (within 9), every ladder's foot and top zone and Ashley's place (within 30), ok or NO. All twelve stages: every event point, ladder and Ashley ok. Not reached: stage 6's crate 6 (276, 37, 285) and stage 10's crates 2 and 3 (239, 92, 259 / 273); no event or clear needs them.
- *The clear chains* (mission flag 0, or the port's own ends), read from the events: stage 2 the emblem's halves (crates 2 and 5, one up a ladder) to the valley door; 3 the waterfall's end; 4 and 11 the giants; 5 the key from record 2, the door, Ashley, back to the entrance; 6 25 kills, which is every enemy of its records (25 entries in all, group 2's one included); 7 the cannon with Ashley after 2 minutes; 8 the fire room (point 6), the statue (point 5), point 6 with Ashley; 9 "locked" at the exit lets group 2 in, record 6 down, the exit; 10 record 2 (group 2, after 3 kills) touched, point 4, Ashley to the exit at y 92; 12 the elevator's time.
- *Stage 11 stopped at its start*: its fog starts at 0 (FogDis 0), and stage_fog() set the table's offset to -1: libnds's assertion "glFogOffset is out of range" stopped the game as the stage loaded. The offset is now held at 0 or more. Checked in melonDS: stage 11 plays.
- *Touch events with a condition*: "inside" was kept whether or not the condition held, so a touch event whose condition came true with Leon already in it never went off until he stepped out and in again. Stage 6's group 2 (point 5, the cabin's floor, after 2 minutes) is one of its 25: staying in the cabin, the stage could never clear. Stage 7's cannon (after 2 minutes) and stage 10's door (just after the key, by the body) are the same kind. Now inside counts only while the condition holds. Checked in melonDS: standing on stage 6's point 5, group 2 comes at 2 minutes (groups 1, then 5).
- Not done: a whole playthrough of each stage. The giants (stages 4 and 11) need enough rounds to bring them down; that was not checked.

**Rounds for the giants (2026-11-11)**: only the guns (and the rocket) hurt El Gigante: 4500 health, 100 a handgun hit; 21 hits bring him down, a cut takes 1500. At best one dies in about 30 handgun hits (21, a cut, 9 more), 45 with no cut; stage 11's two in about 60. A mission entered from the stage select with nothing bought has the magazine and 20 (30 rounds). Stage 4's crates: one always 10 handgun rounds (code 9 with the handgun in hand), another 10 one time in 6: 40, enough with few misses. Stage 11's: handgun rounds one time in 4 (code 7), so 40 at most, short of 60. The shop before each mission sells the handgun's box of 50 for 500 PTAS (Leon starts with 30000), and the rocket launcher fells a giant outright, so stage 11 is cleared by buying rounds first.
- *Retry kept what the failed try left*: a retry (game over or pause) refilled the magazines but kept the rounds carried, herbs and money as they were at the death, so every retry of stage 11 came with fewer rounds, down to where the giants could not be brought down (only Quit, Continue and the shop got out of it). Now what Leon carries in as the mission begins (from the shop's Start) is kept, and a retry starts from it again, as the game's does. `-DAUTOPILOT=70` (3 shots, reload, pause, Retry, YES): checked in melonDS, stage 11: 20 spare, 17 after the reload, 20 again after the retry.

**Foes that never came, again (2026-11-11)**: the placement of *Foes outside the field* / *Foes that never came* (above) was gone from the code: the spawning was reworked for the mission's groups on 2026-11-10 (its turn cut off by a usage limit) and foe_flood() / foe_reachable() / foe_near() were dropped with the old foes_step(). A foe whose point was far off, on another level or out of Leon's reach stood there waiting to notice him. In STAGE 9 as shown (stage 10, the central hall) the first group's three cultists stood in the hall below the balcony Leon starts on, never noticed him, and the 3 kills that let the next group in (with the key's cultist) never came: nothing happened. The three functions are back as they were in the beta of 2026-11-08 (`C:\Users\User\Documents\re4-zeebo-ds-beta`), called from the new foes_step() (once a call, when a foe needs it); one brought in faces Leon and is after him (spotted, not enemies_alert(): that is the leaders' now). `-DTEST_FOE_DUMP` prints the flood ("near ... cells ... y0"). Checked in melonDS: stage 10's three come in on the balcony's level (127, 80, 234 and others) and reach Leon within 20 s; stage 9's two first cultists come in near the start; stage 5's key cultist (record 2, on the gallery) on the ground floor. A check of everything else between that beta and now (every line of the beta's ds/source and re4zeebo missing from the present code, and every Edit of the three sessions' transcripts) found nothing else lost: the rest was changed on purpose (the game's data for damage, health, attacks, drops, the ladders' strips, ranks). The stage numbers in these notes are the port's own (`cur_stage + 1`); the stage select and mission titles skip stage 6.

**STAGE 9 as shown (stage 10, the central hall) could not be cleared (2026-11-11)**: three things.
- *No Ashley*: the mission has her (record type 1, in the hall's alcove at 130, 4, 400) and its way out is "with Ashley" (event 5, GET_WITH_ASHLEY), but she was loaded only in stages 5-8. Now also in stage 10 (ASH_STAGE_E). Without her the way out could never be taken.
- *The hall is a trap*: its floor (y 0-4: the hall, its pillars, her alcove, the door at point 4) has no way up to the balcony and the corridor to the way out (y 65-92); the game's stairs down to the door Ashley opens ("Ashley opens the door Leon is standing behind") are not in the collision. Leon could only drop in, and the key's touches (record 2's body, the door at point 4) were his, so the stage pushed him down there for good. Now in stage 10 he stops at the balcony's edge instead of dropping, and point 4 is also reached from the balcony right over it (68 up). The key's leader (record 2) comes up to him anyway (*Foes that never came*).
- *Her joining*: once the key has opened the door (event 2, scene 10: she throws herself at him), she is with him (placed beside him), instead of still waiting in the alcove he can't get to.
- Checked in melonDS: the audit from the hall's floor reaches no way up (P0-P2 NO, with steps up to 8); from the balcony over the door with the key's event played, "Ashley used the key. The door is open now.", the scene, Ashley beside him; at the way out with her, MISSION COMPLETE and the results; walking off the start's bridge he stops at its edge. From Ashley's place in stage 5 every point and both ends of the ladder are reached. Not played through in one go, nor on hardware.
- `-DAU_STEP_U=n` lets the audit climb steps up to n units (to tell a missing stair from one a little too high).

**The lid (2026-11-13)**: on hardware, after closing and opening the lid (the console's sleep), every texture loaded later (the next stage, a stage clear's pictures) came out white; those already loaded stayed right. The cause was not found (the library's sleep touches neither the 3D engine nor VRAM; melonDS's lid hotkey only paused the emulator, so it was not reproduced). Now the console's sleep is off (disableSleep()) and the game rests itself while the lid is shut (lid_rest()): it stops where it is, both backlights off (the power-management backlight bits, not the DSi brightness level, which the DSi keeps), the power LED blinking slowly, the music silent; when the lid opens it goes on, the 3D settings, VRAM banks, haze and fog set again (gfx_restore()). It draws more battery than the console's sleep (the CPU and RAM stay up). Not checked on hardware.

**From play on hardware (2026-11-13)**:
- *An enemy's mesh across the screen, the frame rate down, now and then after several stages*: enemies_set_look() puts the cultists' one-handed weapon clips (jya_w0) in for a castle stage, and nothing put the Ganados' (ganado_w0) back for a village stage after it; jya_unload() frees that buffer, so an armed Ganado was posed from freed memory. Every stage's enemy set-up now sets the Ganados' own look and clips again. Not reproduced in melonDS (it needs the freed memory reused).
- *Stage 9's fog* (the two Garradors' hall, STAGE 8 on the select): its far walls are often left out (the stage's triangles are cut back with two Garradors drawn), so the fog is far thicker there: from 30 to 170 (the game's 129..940).
- *The shop's layout, as the Android version's*: the top bar holds a framed slot with the chosen row's icon on the left and the merchant's line on the right, by list ("What're ya buyin'?", "What do you wanna sell?", "Whaddya want to upgrade?"; a refusal in pink in its place); the bottom bar has the attaché case and its room in use / size on the left, the money on the right. The game's merchant himself, on the right of its screen, is not in the Zeebo data and is not drawn. Checked in melonDS (`-DTEST_SHOP -DAUTOPILOT=34`).
- *Doors with a key* (2026-11-14): stage 5's chapel door (the Hexagonal Emblem, event 8) opened with A without it: its event's reach (2000: 7.8 units) is less than a door's "A: Open" (12), so one step back from it the door offered to open. Now a mission door with an event by it (an event point within 14 of the door's middle) that wants an item (COND_ITEM), not yet played, is *keyed* (Obj.keyed): shut, solid, no "A: Open" (only the event's "A: Examine": "Locked. I need a key..."), and enemies don't open it either; when that event plays (the key used) the door swings open. Stage 8's dungeon door (its key, event 4) is the same kind. Checked in melonDS, stage 5: without the emblem, no "A: Open" on the way and "Locked" at the door; with it (`-DTEST_ITEM=83`), "You opened the door with the Hexagonal Emblem.", the door opens and Leon walks through to Ashley's room.
- *The knife's view* (2026-11-14): it had come in like the gun's, low over his right shoulder (him at the left edge). In the Android video (13:12, a crate cut) the view is straight behind him, over his head, looking down past him at what he cuts, him in the middle. Now with the knife: 12 behind, 0.5 right, 22 up, looking at a point 40 ahead and 4 below his feet (about 25 degrees down); the tilt (Up/Down) still moves it as before. Checked in melonDS (`-DAUTOPILOT=50`).
- *Drop markers by kind* (2026-11-14, as in the game, from the user): the light column, the star and its glow are yellow for rounds, green for a herb, white for money and treasures (they had all been a pale yellow-white). Checked in melonDS (`-DTEST_DROPS`: the herb's column green, the box's yellow).
- *The battlements' throwers (stage 7, STAGE 6 on the select)* (2026-11-14): the cultists up on the walls could not be killed: the walls' parapet is in the collision, so every shot stopped on it, just short of them. Now a shot that hits a wall goes on 20 units (THROWER_REACH) for a thrower only (enemies on the walls throwing things); other enemies behind a wall stay safe. `-DTEST_THROWER_AUDIT` counts, for every thrower, the floor spots it can be shot from: 106 to 1488 each (the nearest 35 to 53 away); before, none.
- *The weapon screen's pictures in the battlements* (2026-11-14): with the cultists loaded (jya) the texture VRAM is full in that stage, and the status screen's sheet failed to load: its frames and icons were left out. Now, if it fails, the cultists' textures are let go while the screen is up (jya_unload()), and loaded again when the game goes on (if that fails, the Ganados' look). Checked in melonDS (`-DAUTOPILOT=22`): the frames shown, and the cultists textured again afterwards.
- *Drops from crates on tables* (2026-11-14): the item fell straight down from the crate, landing on the table's top or under it (out of reach). Now a crate's item that would fall more than 3 units drops on the floor, on Leon's side of the table (the first spot 2 to 20 units from the crate towards him with floor and no wall, else at his feet). Checked in melonDS (`-DAUTOPILOT=74`, stage 5's table crate): the money lands in front of the table and A takes it (Pesetas +900).
- *Multi Ammo again* (2026-11-14, from the user): the drops had come back as each gun's own rounds (the game's codes 7, 8, 9, 12, 13: "Rifle Ammo +3", "TMP Ammo +50"...), where the port had settled on one kind (the beta: "Multi Ammo +10" for the gun in hand). Now every ammo drop is Multi Ammo: taken, it gives the gun in hand that gun's box from the game (handgun 10, Magnum 5, shotgun 5, rifle 3, TMP 50; with the rocket launcher in hand, the handgun 10), its icon showing the box of the gun in hand. Which codes leave ammo, and how often, is the game's as before. Checked in melonDS (`-DAUTOPILOT=63 -DTEST_DROP_CODE=8`, code 8 that left rifle / TMP / Magnum rounds): "Multi Ammo +10" with the handgun in hand.
- *A Ganado in Ashley's room (stage 5)* (2026-11-14): a foe brought in near Leon (*Foes that never came*) took the place of the flood farthest from the others, and the flood went through shut doors (it only knew the stage's walls): the room behind her door, out of the way, was where they came in. Now a step through a shut door (or into a crate, a barrel) is taken only after the rest, and what lies behind it is marked: still reached (a foe whose point is there walks round and opens the door; foe_reachable() is as before), but no foe is brought in there. `-DTEST_ASH_ROOM`: from six spots round the chapel, 30 to 36 cells of her room had been places to bring one in, now none (the flood still reaches her place).
- *Story lines one off* (2026-11-14): the translation (re4zeebo/cutins_en.txt) had skipped a short line in four places and numbered everything after it one up, so the pages' speakers (from the scripts) were the next line's: stage 5's rescue ("Ashley!" missing: "Stay back!" was Leon's, "Hey! Calm down, Ashley..." hers), stage 2's 3-23 (Luis's cell: "..."), stage 7's 15-34 (Luis with the drug: "Luis.", "Drug?", "Uh-huh...", "Yeah..."), stage 10's 16-32 (Luis dying: "Here...", "Luis!"); stage 2's 28 had no line of its own. Each English line checked against the Portuguese of its number (all 364); cutins.c regenerated.
- *STAGE 6 (the battlements) had few enemies* (2026-11-14): its records are as the Zeebo file has them (18 entries, 2 more only on the harder setting), but group 2 (10 of them) comes in when Leon reaches the cannon with Ashley, and the cannon fired as soon as he did: its 2 minutes (event 9, a time condition) were counted from the mission's start. In the game a time condition reads GM_MisConduct::Get_LimitProgressTime() (libbio4af.so: _Check_Cond_Time), the seconds since a script's command 205 (_Cmd_Ev_SetLimitTime -> Set_LimitTime: the time back to 0, counting up to its value; Frame() counts it once a second, and at the end a kind-2 timer clears the mission, a kind-1 fails it). Stage 7's 205 (120 s) is in "Don't worry. I'll hold them off. Ashley, get it ready." (event 8, A at the cannon after event 10). Now command 205 is kept (OP_TIMER) and the time condition counts from it, in vblanks (real time: the stage runs at 30 frames a second, which had made 2 minutes take 4). Stage 1's (180 s at the start, 240 s at the chainsaw man), stage 6's (240 s at the start) as before but from those; stage 12 keeps its timer from the start (its Verdugo scene plays there). The kind-2 / kind-1 ends are not done (the port's own ends stand). Checked in melonDS (`-DTEST_SHOW_FOES -DTEST_SHOW_TIMER`, at the cannon with Ashley): group 2 let in on arriving, the timer from "I'll hold them off", the cannon 2 minutes after, mission complete. (The script command order, from 200: Message, DrawImage, GetGold, GetItem, ThrowItem, SetLimitTime, SetMisFlg, SetEveFlg, DeleteEv, PlayEv, ChEntryGlp, ChDelete, ChMvtPlay, ChSetAiMode, ChSetTrgGoal, ChSetPos, ChTurn...)
- *Leon out of the picture by walls* (2026-11-14): with his back to a wall the camera was brought in to 2 behind him and rose 25 over his feet, still looking 40 ahead: he was right under it, out of the picture (and faded). Now (cam_clear()) when a wall brings it in closer than 9 it swings round him, up to 60 degrees to the side with the most room, eased, and back once there is room; swung, and pressed in close, its look-at comes in to 6 ahead of him. Not while aiming. Checked in melonDS: stage 7 at the cannon facing the parapet, stage 5's corner by the ladder facing each of 4 ways, a bot run through the church: he stays in the picture.
- *Stage 3's Plaga Ganado* (2026-11-14, from the user): the mission's first Ganado (record 2, at 139, 4, 276) is kind 18 of ch_param_2: 3000 health, the head 240 %, equipment category 19, and +25 set, the kind's WithPlgFlg (GM_Data_ChP_Info::Get_Idv_WithPlgFlg in libbio4af.so reads +25 of its 28-byte kinds; +26 the Plaga's own kind). The game's picture of the scene (eve_stg03 #2: "A Ganado stands in front of Leon... Its head is grotesque") shows him headless, a Plaga with blades rising from the neck: eqp_plg_all model 0 (82 triangles; its skeleton of 15 nodes, clips #0-4 of eqp_plg_all.mcas: 33, 47, 19, 53, 53 frames; #5-8 are model 1's, 27 nodes). It had been drawn as an ordinary Ganado with a hatchet. Now FoeKind.plaga (gen_stage_foes.ps1, +25; set on Ganado kinds 18 and 37, cultist kinds 13 and 27, only stage 3's record uses one) makes enemies_set_kind() give it no weapon and no head (node 19's rotation zeroed: the head's vertices drawn to a point) and the Plaga is drawn on its neck (node 18, model_draw_on()), animated: clip 0 standing and walking, 2 attacking, 1 hit, 4 going down (the game picks them from the body's motion, Get_ActionNo, not followed here: chosen by eye). Shots hit it up to 26 above the feet (the Plaga stands tall; above 14.5 is the head's damage). Loaded from NitroFS (plg.bin, plg_anim.bin) only on a stage with such a kind. Its attacks are the bare-handed Ganado's (the Plaga's own lash, by its motion table, is not done). Checked in melonDS (`-DTEST_PLG_VIEW=30 -DAUTOPILOT=75 -DAUTOPILOT_TILT=10 -DTEST_INF_AMMO`): headless, the Plaga swaying and lashing on its neck as it walks up, hits on it, it goes down.
- *The knife on things up on tables and shelves* (2026-11-14, from the user): with the view level the knife always played the crouching low slash (ch_leon_w00 #3, for a crate at his feet), and the blade's line ran from his chest (13 above his feet) straight out: stage 5's table crate stands at 13.4, just over it, so the cut missed too (he ducked and slashed under it, nothing broke). Now the blade meets objects in a band, its line and lines 4 above, 4 below and 8 above (knife_object()), and when it meets one standing more than 5 over his feet (KNIFE_RAISED) the chest-high slash (#1) plays, as when tilted up. Tilted down, or nothing raised there, as before. Checked in melonDS (`-DAUTOPILOT=76`, stage 5's table crate, level): the chest-high slash, the crate breaks and its money lands in front of the table. `-DTEST_SHOW_KNIFE` shows the nearest crate and what the probe met.
- *Unused models* (2026-11-14, from the user: the models in the data the port did not use, against the game). Of re4zeebo/out's 85 model parts, unused were: eqp_wep_all #8, #9 (grenades: the port has none), #12, #13, #15; eqp_plg_all #1 (the other Plaga, 27 nodes: no record uses a kind with it); eff_all's 19 effect meshes (the port draws its effects as sprites from the same textures; #17, the bolt, now used); map_ico_all and map_stg_NN (the map screen's: the port has none); _deb_bound_* (debug). GM_Data_ChParam::Get_WepMdlId (libbio4af.so, a jump table on the equipment category) gives 13 #10, 14 #11, 15 #12, 16 #13, 17 #14, 18 #15, 19 #16 (none in the Zeebo data), 20 #17, 21 #18. The castle's records use cultist categories 15 (stages 8, 10), 16 (8, 9, 10), 17, 18 (7, 9, 10: the leaders) and 21 (stage 7's throwers); all but 17 had carried a stand-in sickle. The video (Zeebo playthrough FSo_7vrThNY, missions 7-9 at 59:10, 1:11:56, 1:24:00) is too small to make the cultists' weapons out; the data is followed.
  - *Category 15* (#12, a board set with spikes): ch_jya_w01 (6 clips: #0 standing, #1 walking, #5 the swing, 48 frames); its attack script 76 (motions 59-61, condition 12: 20): steps 150 a tick on frames 1-10, 450 on 12-29, the blow on 20-22: 1800, heights 1000-3500, +-1000, 480 (40 of Leon's 100; 680 on the harder setting).
  - *Category 16* (#13, the crossbow): ch_jya_w02 (9 clips: #0 standing, #1 walking, #2/#3 aim poses, #4/#6 reloads, #5 the shot, 12 frames); its scripts (90-97) shoot (command 105, GM_Hdl_Ch_Normal::_ObjReq_BullShot -> a bullet, GM_Hdl_BullBase::_Main_Move_Shot) and have no blow. In the port it stops 25 off while it sees its target and shoots at Leon (or Ashley, the nearer) within 80, about ahead, in sight, every 200-290 frames; the bolt (eff_all #17) flies straight at 90 units a second from 13 up at where the chest was, a wall stops it, a body within 2.2 takes 240 (20; the bolt's own damage and speed were not found: the port's, as its dynamite and rocks). (`TEST_GOD` builds shoot too, harmlessly.)
  - *Category 18* (#15, a staff topped with a horned skull, the leaders'): held in the hand; its motions (94-102) are the bare hands' scripts (40-47), so it takes hold and lunges as before.
  - Loaded with the cultists (jya_load(): cboard, cxbow, cstaff, bolt, jya_w1, jya_w2 from NitroFS, about 10 KB of textures); if they can't be, those cultists use the base clips with nothing in hand. A blow's damage is now the attack's own (enemies_blow()). Checked in melonDS (`-DTEST_CULT_VIEW=n -DTEST_CULT_DIST=50`, stage 9: n 4 the crossbow, 3 category 15, 28 the staff): the crossbow held, bolts flying and hitting Leon; the board swung; the staff carried. Stage 7 (its VRAM the fullest): the start and the weapon screen as before.
- *The bottom screen's map* (2026-11-14, from the user: the vital display made a map). The game has a plan of every stage (map_stg_NN.mcms, 21-67 flat triangles in world units, in groups draw_01..04: the floor, the floor above (stage 5's gallery and Ashley's room, stage 1's houses), what stands on them) and map icons (map_ico_all: plain squares and bars). re4zeebo/gen_stage_maps.ps1 turns the plans (the unpacking's OBJs, stage 12's moved as its stage data) into ds/source/stage_maps.c; vital.c draws the stage's plan, fitted to the panel under the header bar (x right, z down: seen from above), its groups shaded, with Leon's arrow (yellow, his facing) and Ashley's dot (pink), redrawn where they move. The map is now the bottom screen's default in play; a tap on the header bar switches between it and the item select (the herbs' double tap is there), and back. Checked in melonDS: stages 1, 5, 7, 12 (Leon at the start where the stage starts him; Ashley in her room in stage 5), a bot walk in stage 5 (the arrow follows him, turns with him); after a story scene the map is laid out again. Not done: the original's icons (the port's own markers), the floor Leon is on (the plan shows all floors at once).
- *Memory note* (2026-11-14): with the cultists' other weapons loaded, a `-DTEST_BOT` build of stage 7 no longer starts (the bot's large tables); `-DTEST_NO_CW` leaves those weapons out for such runs. The normal build has over 4 MB free in stage 7 in melonDS (`-DTEST_SHOW_HEAP`: the largest block, tens of KB, in the FPS field; DSi mode).
- *The map, turned round Leon; the shortcuts down the side* (2026-11-14, from the user: his marker fixed and the map moving; the map need not be in front; the item select's shortcuts, smaller, down the left). The bottom screen in play is now: BG3 (VRAM C, an affine 16-bit bitmap 256 x 256) holding the stage's plan, drawn into it once per stage (fitted to 240 texels with a margin), and turned and moved by the hardware each frame (vital_commit(), after the vblank wait: PA..PD, X, Y) so that Leon stays at (150, 118) and his facing points up the screen, one screen pixel a world unit; over it sub-engine sprites (VRAM I, unused before, 16 KB: 16-bit bitmap sprites): Leon's arrow (fixed, pointing up), Ashley's dot (placed by the inverse turn), the item shortcuts down the left (two 32 x 64: the gun in hand, its rounds, the herbs, each its icon and count; a double tap on the herbs uses one, as before) and a note strip along the bottom (six 32 x 16: "Life restored.", "Life is already full.", items got). The full item select screen (and the header's map / items switch) is gone. Story scenes and the menus draw on BG3 as a plain bitmap (bg_plain(): the turn undone, the sprites hidden), and the plan is drawn again when play resumes. (The sprites' pictures go by DMA from static buffers, flushed from the cache first: DMA can't read the stack in DTCM, where a first try's arrow had come out blank.) Checked in melonDS: stage 5 (a bot walk: the map turning and moving under the fixed arrow, onto the gallery's colour when he goes up; Ashley's dot), stage 2 (`-DAUTOPILOT=12 -DTEST_HURT`: the herbs' double taps 3 -> 2 -> 1, "Life restored.", then "Life is already full."), stage 4 (`-DTEST_OPENING`: the opening scene, then the map again).
- *The shop's ammo, Multi Ammo too* (2026-11-18, from the user): the drops had been made one kind (*Multi Ammo again*, 2026-11-14) but the shop's buy list still had a row for each gun owned ("Handgun Ammo", "Magnum Ammo"...). Now it has one row, Multi Ammo: a box for the gun in hand as play will start it (the rocket launcher in hand: the handgun's), that gun's box and price (handgun 50 for 2500, as before), its icon that gun's box. Checked in melonDS (`-DTEST_SHOP -DAUTOPILOT=34`, the Magnum bought): the list ends "Rocket Launcher, Multi Ammo x50 2500 PTAS", no Magnum Ammo row; bought, the case's room in use 16 -> 18.
- *Options, the status screen's plate, bonus notes* (2026-11-18, from the user):
  - *The status screen (Weapons)* shows Leon's plate again (the ammo counter and the long life bar), just above its money bar (draw_plate(), ST_PLATE_DY 29 up from its place in play); it had been taken out ("no life bar here").
  - *The pause menu's options* gain two of the port's own, before Controls: **HUD** (Show / Hide: Leon's plate and life bar and Ashley's plate; the hurt flash, the pickups' popups, prompts and notes stay) and **Bottom Screen** (Map, the default / Item Select: the earlier screen, the condition, heartbeat, the three slots and the description, brought back as vital.c's items_update(); the map, its sprites and turn put away while it shows). For the session, as Sound (not saved). Checked in melonDS (`-DAUTOPILOT=77`: Options, HUD -> Hide, Bottom Screen -> Item Select).
  - *Bonus notes* at the top left, in English, as the game's GM_MisConduct::Set_Bounus shows its effect (libbio4af.so: kind 1 on a hit whose result is a critical one, 3 in GM_Obj_ChBase::Set_AimDamage; kind 2 on result 4, a weak point (Get_Prts_WeakPointFlg); kind 0 from an explosion (GM_Hdl_Fix_Normal::_Set_Exp) and enemies' throws and shots; "Joia Adquirida!" for a treasure, in the video): "Critical Damage!" (a critical head shot, enemies_shot()'s crit), "Technical!" (a weak point: the Plaga's, a Garrador's back, El Gigante's tentacle cut), "Strategic!" (enemies felled by a barrel's or a canister's blast, not the rocket's), "Treasure Acquired!" (a treasure taken). Gold, two at most, 1.5 s each (text lines 12-13). The game's own wording in English, and which hits count as its weak points, are the port's reading. Checked in melonDS (`-DAUTOPILOT=63 -DTEST_DROP_CODE=14`: "Treasure Acquired!" with the treasure's popup); the other three not seen in a run.
- *Multi Ammo, one count* (2026-11-18, from the user: the counts were not one). The drops and the shop had been made "Multi Ammo" by name, but each gun still kept its own rounds (wep_spare[]): taking up another gun brought up its count, and a pickup went to the gun in hand's. Now the rounds carried are one count (spare) for every gun but the rocket launcher (its rocket is its own: it never loads from the count; USES_ROUNDS()): taking up a gun keeps it, a reload of any gun draws from it, a drop adds 10 (MULTI_DROP), the shop's box is 50 for 2500 (MULTI_BOX, the handgun's), the case counts it in boxes of 50 (2 each), the status screen lists it in boxes of 50 with the gun in hand's icon, the retry and the save keep the one count (a save from before: the guns' rounds added together). wep_spare[] is gone. Checked in melonDS (`-DAUTOPILOT=33 -DTEST_ALL_WEAPONS`: 120 rounds with the handgun, still 120 after equipping the shotgun; its icon the shotgun's box).
