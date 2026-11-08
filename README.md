# RE4 Zeebo → DS demo

A Nintendo DS / DSi demo built from the *Resident Evil 4 Zeebo Edition* data:
Stage 1 (the village) and Leon, with his animations, the handgun, floor/wall collision,
fog and an over-the-shoulder camera — and five Ganados who walk up to Leon, stagger when
shot (1 damage, 2 for a head shot, 3 HP) and go down.

The Ganados fight back, one at a time with a pause between attacks (each Ganado waits 4–5.5 s
before its next one, and Leon is safe for 1.5 s after being hit): a swipe (15 damage, Leon
flinches) or a lunge that grabs him. While held he loses 8 HP every ~1.7 s; `Mash A: Struggle!` blinks (the game's "Ação: Eu luto") and 8 presses of A/B/X/Y break free: Leon shoves him off and kicks (`ch_leon_base` #21), and the Ganado reels back dazed, so `A: Kick` follows as after a head shot;
otherwise the Ganado lets go after 4 s and Leon only staggers free. Ganados are solid: Leon can't walk through them.

Screens and HUD use the game's own 2D graphics and font (Japanese text):

- **Title**: the title art (`assets/title/title_ds_edition_source.png`: the Zeebo picture with the subtitle changed to *Nintendo DS edition*; the untouched originals are next to it) with a menu, ゲームを始める (start) / 操作方法 (controls list).
- **Stage select**: the original's 12 stage pictures (`2d_stage_00`) in a grid; the chosen one
  is drawn larger in a pulsing gold frame. Only stage 1 (村, the village) is playable; the others
  are dark, say 準備中 (in preparation) and shake with a click when chosen. Confirming zooms the
  picture to fill the screen, then fades into the stage.
- **Item drops**: a Ganado going down leaves an item 70% of the time (more likely a herb when
  Leon is hurt): multi ammo (+10 rounds for the gun in hand) or a green herb (carried, up to 9; used from
  the item screen for +40 health). It shows as its
  icon (`2d_itemic_00`) floating in a light column with a twinkling star (the original's glow
  textures, `fix_all` #7 and #0, made translucent); walking over it takes it, with sparkles
  (`eff_all`), the pickup sound and a popup. Rounds carried (残弾) show above the plate; reloads
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
  below). Double-tap the herbs to use one (+40 health;
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

**El Gigante** (stage 4; two in stage 11, as mission 11 has it: the second starts 40 degrees to Leon's left and 30 units farther, sharing the first one's model (`giant_share`); they keep apart, each is shot and cut on its own, and the stage clears when both are down): the game's model and animations (`ch_elg`, from NitroFS; 45 bones, the last 17 folded in). He walks at Leon and swings (45 damage; a blow shakes the view). 8 points of damage (8 handgun hits, 2 rifle shots) bring him to his knees with the parasite out of his back: walk up and press **A** (`A: Climb`, the game's "Ação: Escalar"). Leon has no animation of climbing him in the game's data (none of his clips but the ladder climb moves him up); the Zeebo version shows it as a story scene (see **Story scenes** below: scene 11, its picture of Leon on top of El Gigante and two lines). The port does the same; then Leon is behind him, facing him. He also has 60 points of life that every shot wears down (handgun 1, rifle 5) and a cut takes 20 of: the rifle can finish him, as in the video. Three cuts fell him too; he stays down 12 s, counted from when he is down (time to run up from where he was shot), and gets up if not cut. Three cuts fell him; the stage clears when he and the Ganados are down. (In the Zeebo video the stage-4 fight is the mine arena with Ganados around, the giant shot until he drops and the parasite cut.)


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
**The Garrador** (stage 8): the game's model and animations (`ch_gal`, from NitroFS; 33 bones, the last 5 folded in). As in the Zeebo video (a cell in the castle, "he can't see Leon, but he can hear his steps"), he is blind: he stands listening, and charges at any shot, at Leon running within 60 units, or at Leon right beside him. Leon in reach at the end of a charge gets his claws (40 damage); a charge into a wall leaves his claws stuck in it for 2.5 s. Shots in his back (the parasite) do three times the damage: four handgun hits there fell him. Ashley is with Leon here too (the video shows her by his side past the cell): his claws catch her as well if she is in front of him. (His texture is halved, 64x128, to leave texture memory for her.)

**Missions and their enemies** (the game's mission list, `common/stgnfo_mission_ms.bin`): 1 Sede de Sangue (village), 2 Luis Sera (valley), 3 Las Plagas (waterfall), 4 A Gaiola do Gigante (mine, El Gigante), 5 O Resgate de Ashley (church), 6 A campanha no cerco (the villagers besiege the cabin), 7 Para o Grande Castelo (battlements), 8 A Arma Cega (audience hall, the Garrador), 9 A Dupla de Guardiões (two Garradors), 10 Reunião (central hall), 11 A Fornalha (two El Gigantes), 12 O Assassino mais Forte (old irrigation canal, the Verdugo). Who is in each comes from the mission file's enemy records (see below): the Garradors (one in stage 8, two in stage 9, shared model `garrador_share`), the giants (one in stage 4, two in stage 11) and the Verdugo stand on their records' points.

**Enemies noticing Leon** (stages 2-12): each stands on its spawn point facing the way its record gives it (256 to a turn), instead of every one heading for Leon from anywhere at once. One notices him within 40 units whichever way it faces, or within 150 in front of it (about 60 degrees either side, not far above or below), or when shot; those within 80 of it notice too; once it has noticed him it keeps after him. (Stage 1 keeps its own scheme.) `-DENEMY_SIGHT=inttof32(n)` changes the 150.

**Collision and walls** (all stages): the game's own collision data (`stg_data_NN.bin` section 1) starts one word later than first read: every triangle had been read shifted by one coordinate, so the walls taken from it stood in the wrong places. Read right, its walls block where the game's do and its floors (ledges, walkways, stairs) are added to the ground, except the huge flat planes under whole maps; stage 12's data is moved by the same 468, 988 as its mission points. **Camera**: a ray from Leon's head to the camera brings it in short of any wall between them, so turning with his back to a wall no longer puts the camera behind it (where the wall faces away and isn't drawn). **Vertex budget**: models keep the game's triangle strips (`BEGIN_VTXS` strip, neighbours share vertices) instead of separate triangles, about 30% fewer vertices; the DS stores at most 6144 a frame and 2048 polygons, and stages with Ashley had reached 6144. `-DTEST_GX` shows the load on the vital screen (FPS field: polygons; spare rounds: vertices; herbs: frames over the limit in the last second; Loaded a/b: the stage's own polygons / vertices). `-DAUTOPILOT=20` (+ the barn-wall start) turns Leon round with his back to a wall.

**Drawing speed**: every character model's draw list (texture, polygon format, vertices) is sent to the geometry engine by DMA, in the background, like the stage model; the CPU poses the next character meanwhile, and waits only before anything else is drawn (`model_sync()`). With this, stages 6, 7 and 8 (with Ashley, the cultists and the Garrador) run at 56-60 fps in melonDS (before: about 30, 42-55 and 35-40). The stage clears when he and the cultists are down.

**The Verdugo** (stage 12): the game's model and animations (`ch_ver`, from NitroFS; 49 bones, the last 21, his tail and fingers, folded in). His moves follow the original game: he stalks Leon, running when far, and strikes with his tail (50 damage); every 9 s or so he springs up out of sight and drops down beside Leon. The handgun barely hurts him (120 hits). **Liquid nitrogen tanks** (the barrel model, read in for this stage) stand on the mission file's kind-06 points (4 listed; 3 of them have floor in the stage data and are placed): shoot one open and the white cloud freezes him within 22 units for 5 s, and each hit then counts 20 times (about six shots). A tank doesn't hurt Leon, and tanks don't set each other off. The stage clears when he and the cultists are down.

**The mission files** (traced in the game's module `bio4_brew.mod`): the 16-byte points (read at 0x45c54) are the stage's *objects*. Each becomes an actor whose kind picks its handler (0x2968c, 0x1b30c) and what a shot does (0x49c48): 01 a hinged door that swings open either way and shuts again (0x1bb00), 02 a ladder (`fix_all` #2; kinds 1 to 6 and 9 all use the part of their own number), 03 an explosive barrel (big blast, 0x4a688 size 3), 04 a breakable object that drops the item its byte 2 names, 05 a canister on chains that falls when shot and blows up (size 5), 06 the liquid nitrogen tank, 07 a treasure (knocked down), 09 something that rings when shot (sound 18; stage 8's notes speak of a bell that makes a loud noise). The enemies are the section at header +8 (read at 0x44b54): 10-byte records of wave, a setting flag (0 always, 1 and 2: the same enemy in two versions, 2 left out here as the harder one), type (0 Leon's start, 1 Ashley there / -1 not, 2 Ganado, 3 cultist, 4 the chainsaw man, 5 Garrador, 6 El Gigante, 7 Verdugo), a variant, a spawn point number (into the stage data's section 3 points, `stg_data_NN`), facing (256 to a turn), two bytes not understood, the drop code (byte 8, the table below; 5 = money is the commonest) and a last flag. **Stages 2-12 follow them** (`ds/source/stage_foes.c`, written by `re4zeebo/gen_stage_foes.ps1` from the files): Leon starts on the type-0 record's point, facing its way; the Ganados / cultists come in wave by wave, the first standing at their points from the start, each later wave once every enemy before it is down (an assumption: the game's own wave trigger was not traced), at most 5 standing at once, and later ones taking the enemy slots of bodies that have gone (there are up to 22 to a stage, 12 slots). Several of a wave on one point come out side by side. The bosses stand on their records' points. The kind-04 objects are crates (break them: the item they name) and the kind-03 ones explosive barrels. Two of stage 7's three stand at height 85 where its map has no floor, beside the four cultists whose records are up there too (the only enemies that high): those four stay at that height and throw burning rocks (the mission's notes: at the word of the men on top, the others hurl burning stones; that they are the throwers is this port's reading), and neither the waves nor the stage clear wait for them. A barrel going off beside them fells them. The game's cannon on that stage (Ashley helps Leon fire it) is not built. Stage 1 keeps its own scheme (its records give 21 enemies in 8 waves, the chainsaw man in the second).

**Doors and bells** (from the mission files' objects): a kind-01 object is a door on its hinge, drawn with the game's door model (`fix_all` #1, its leaf 13 units along its -x): one in stages 2, 5 and 8, three in stage 12, each standing in a doorway of its stage. A at a shut door (A: Open) swings it a quarter turn away from Leon; a Ganado walking into one pushes it open too; it shuts again 4 s later, not while Leon is in its way (the game's door, at 0x1bb00, swings either way and shuts again with a sound; its exact timing was not traced). A kind-09 object is a bell (`fix_all` #9, hanging from its point: two each in stages 8 and 9, in wall alcoves): a shot rings it (the church bell sound) and it swings; the Garradors hear it wherever they are and charge at it (stage 8's notes: "if you strike it, it makes a loud noise"). Stage 12's liquid nitrogen tanks now use the game's own cylinder model (`fix_all` #6) instead of the barrel. The three models are read in only for the stages that have them.

**The hanging canister** (kind 05, stage 8 only): a yellow-labelled canister on three chains (`fix_all` #5), hanging over the long hall at the start, 36 units above the floor. A shot drops it, chains and all (the game pushes the whole model down, harder each frame: 0x3f910), and it blows up on the floor (0x4a688 size 5: 20 units, against the barrel's 60). Here the blast fells Ganados and cultists within 20 units on its level and hurts Leon within 10 by the barrel's 30; the Garradors hear it and charge at it, but are not hurt (their damage from it was not traced). The game's own chain animation is not played.

**Ladders** (kind 02, stages 2 and 6): two in stage 2, three up to the second-floor windows of the stage 6 cabin. The point is the foot of the ladder and its facing has its back to the platform: at all seven points in the files (stage 1's two included) a floor at about 32 lies 16 units behind it, and `stg_data` section 4 holds a landing point nearby. A climbs up or down as on stage 1's ladders. Enemies do not use them. Stage 1 keeps its own two, from action points: one matches the mission file's (319, 68), the other stands at (440, 60) where the file has (440, 214). The follow camera no longer stays up on an upper floor behind Leon after he climbs down.

**Drops** (stages 2-12): the drop code of an enemy record (byte 8) or of a kind-04 object (byte 2) is what it leaves (ff: nothing). The game's module (`bio4_brew.mod`, ARM: the mission loader copies the 16-byte points as they are; the drop table is at 0x49f48, each roll `rand() % n`) reads it as: 1 green herb, 2 first aid spray, 3 hand grenade, 4 flash grenade, 5 600-1000 PTAS, 6 1100-1500 PTAS (steps of 100), 7 handgun / shotgun / rifle / TMP ammo, 8 rifle / TMP / magnum ammo or mine darts, 9 ammo for the weapon in hand, 10 a herb, a spray or nothing, 11 an egg or nothing, 12 and 13 nothing, ammo, a grenade or a herb. With only the handgun here, every ammo is a handgun box, the spray and eggs are herbs, and grenades leave nothing; money is picked up like an item ("Pesetas +n PTAS", shown with the pearl icon) and counts towards the results' PTAS. Code 0 leaves nothing; in the game ff leaves nothing too, but the port gives an enemy with ff its random drop (70%), since Leon has no other weapon or shop to fall back on.

**The elevator** (stage 12): the only story mission against the clock. None of the mission files carries a time limit (the "within the time limit" objectives belong to the Mercenaries-style mini missions, `mis_data_mNN`), but stage 12's notes say the elevator takes "about 4 minutes" to come. Its arrival ends the mission, whether or not the Verdugo is down. The Zeebo video (Verdugo around 2:18:20, Leon in the elevator at 2:20:20) shows no count on screen, so neither does this port: the objective says "The elevator takes about 4 minutes." and "The elevator has arrived." comes up with MISSION COMPLETE. The 4 minutes run while playing, not while paused.

**Collision** (every stage): only the game's own data, `stg_data_NN.bin` section 1. It is u32 groups, u32 triangles, u32, then 16 bytes per group `{ u8 id, u8 walk, u8 camera, u8 shots, u8 kind, 3 bytes, u32 first, u32 count }`, then 36-byte triangles. In `bio4_brew.mod`, 0x64fc tests walkers against the groups with the walk byte set, 0x5728 and 0x71b8 the camera byte, 0x5c80 the shots byte. That those three bytes mean walking, camera and shots rests on this code and on the groups' shapes; which actors carry the flag that selects the camera set was not traced. Leon walks on the walk groups' up-facing triangles and is stopped by their steep ones. The camera tests the camera groups and shots the shots groups, kept in the ground file after the walking data, each piece with a mask. A steep piece is tested as an upright rectangle around it. Groups of kind 1 and 2 have no bytes set and stop nothing (kind 2: door planes; kind 1 is read by the game's shot code). The picture's triangles are no longer used: they were what made invisible walls and walkable roofs.

**Enemies**: every stage has its mission file's enemies again (`-DSTAGE1_ENEMIES_ONLY` takes those of stages 2-12 away). One whose point is where Leon stands (stage 8's first cultist is on his start; stages 2, 5 and 9 send later waves in by the way he came) waits until he is 30 units off it, at his height, or 20 s at most. Stage 7's burning rocks hurt Ashley a third as much as Leon (she ducks; at 30 a rock a few volleys ended the stage).

**Ladders and doors**: the ladders are the mission's kind-02 objects, plus stage 5's, which is part of the chapel's model (its foot and top are stage data section 4, points b3, b4). Ashley starts at the mission's point for her and waits until Leon comes within 30 units; once Leon has gone up or down a ladder she walks to it and climbs it after him with her own climb (`ch_ash.mcas` #11; down, played backwards). Only where there is no ladder (a drop), or after 8 s without reaching it, she appears beside him. A mission door with a wall right across its doorway (stage 2's, the one Leon came in by) stays locked.

**Weapons**: the handgun (`eqp_wep_all` #1, `ch_leon_w01`), Magnum (#5, `w05`), shotgun (#2, `w02`), rifle (#3, `w03`: the one with the scope) and TMP (#4, `w04`), each with its own rounds, bought in the shop; and the rocket launcher (#6, `w06`; 30000 PTAS, as `item_base_info.bin` has it). The parts were told apart by their pictures (`-Preview`); #6 had been taken for the rifle before. Power and rounds are item_weapon_info.bin's first level. Change with Equip on the status screen, or L+X. Each gun is drawn node by node off the hand (`model_draw_held`), with its muzzle at its barrel-end node. The aim poses are made by the exporter (`6~4@50`: clip 6 turned 50% of the way to clip 4, node by node as quaternions); the rifle's set has no up and down poses, its one aim pose is #3. **The rifle's scope**, as in the Zeebo video: fully aimed, the view is from Leon's eye, 20 degrees across instead of 70, darkened all round a clear round lens with red cross hairs; he isn't drawn, and the shot goes along the view (Up/Down tilt it). **The rocket launcher**: one rocket, flying 5 units a frame to where the sight points; its blast (30 units) fells the Ganados, El Gigante and the Garradors and hurts the Verdugo, then the launcher is gone and the handgun is in hand. The shotgun also hits Ganados in its spread. Not in yet: the guns' own moving parts (`eqp_wep_all.mcas`), their own shot sounds, the scope's zoom.

**Shop**: between the stage select and the mission, as in the game: Start, Buy, Sell and Upgrade tabs with the merchant's lines (`Bio4_StringLayout.bin`, in English). Leon starts with the handgun and 30000 PTAS; the other guns, ammo, Green Herbs and larger cases cost what `item_base_info.bin` says, an ammo box holding what `item_ammo_info.bin` says. Things sell for half (the equipped gun can't be sold). Upgrades are `item_tuneup_info.bin`'s (`re4zeebo/gen_shop_tune.ps1` writes `ds/source/shop_tune.c`): firepower, capacity, firing speed, reload speed. Firing and reload speed have no base value in the tables that was identified, so 0.5 and 1.25 x the first level are assumed. Money, rounds, herbs and upgrades carry on between missions and reset on New Game; they are kept in the save file. Different from the game: one press buys one box instead of asking how many; everything is on sale from the start (the game's list grows with the missions, not decoded); grenades, sprays, vests and the rest aren't sold, as the port has no use for them yet; the merchant isn't drawn. Damage is counted in tenths, so weak guns land a point every few hits. `-DTEST_SHOP` starts in the shop with money, `-DAUTOPILOT=34` walks through it, `-DAUTOPILOT=35` comes to it from the title, `-DTEST_ALL_WEAPONS` gives every gun.

**Saving**: `re4ds.sav` (72 bytes) at the root of the card the ROM runs from, through DLDI: money, guns owned, upgrades, case, rounds, herbs, the weapon in hand. It is written when a mission ends, on leaving the shop and on leaving an empty stage, to `re4ds.tmp` first. With a save on the card the title offers CONTINUE; New Game replaces it at the next save. On a card without a file system nothing is saved. To try it in melonDS, enable DLDI with folder sync and close the emulator normally (the folder is written on closing).

**Polygons**: the DS draws 2048 polygons a frame and silently drops the rest, so what is sent last vanishes: villagers and the laser sight. The stages are cut by the exporter into chunks by place (72 units a cell, `SplitChunks`, flag 2 of the RDS1 file). `model_draw_near()` leaves out what is wholly behind the camera and sends the rest nearest first, one DMA transfer after another from the DMA interrupt, up to a budget. The budget follows what the hardware really kept the frame before (`GFX_POLYGON_RAM_USAGE`, which counts only polygons facing the camera and on screen): it starts with the whole stage, shrinks when a frame reaches 1940 polygons and grows again below 1800, never under 1200 triangles (at 600, as on stage 9's balcony over the hall, the floor and walls near Leon went missing on a real DS). Past that the farthest enemies are left out (nearest first, down to 2), and they come back when there is room; the stage is sent first, so what the hardware drops when a frame is still too full is what comes last, not the walls. (A budget reckoned from the models' triangle counts was far too small, as most of those are culled: whole walls went missing.) `-DTEST_GX` shows the count.

**Carpets and floor decals**: a draw lying on another draw's larger floor (most of its flat triangles within 0.3 above that floor) is raised 0.15 by the exporter (`MarkDecals`), because the DS's depth buffer can't tell surfaces that close apart and showed them in patches (the chapel's carpet). Found: 1 or 2 a stage in stages 5-11, 3 in stage 12.

**Ashley's life**: while she is with Leon, her plate from the HUD sheet (`2d_gms_00`: her name, a life track, the round command icon) is drawn above Leon's, its track coloured as his. The icon shows her following, or the second icon while she waits at her mission point. `-DTEST_ASH_HP=n` starts her hurt.

**Reach check**: `-DTEST_SHOW_POS` shows where Leon stands; `-DAUTOPILOT=30` presses A and walks on (doors, ladders), `-DAUTOPILOT=31` cycles the weapons and fires; `-DLEON_START_X/Z/YAW/Y` set the start.

**Treasures** (every stage): the two kind-07 points of each mission file, up on eaves, ledges and beams, glint with the original's pickup star and the ruby icon (`2d_itemic_00`, added as a row of treasure icons to the items sheet). One within reach of the floor is walked over; one up high is shot down (it falls to the floor below, clear of the walls). Each is worth 3000 PTAS (the video's results show "Joia +3000"); the results screen lists TREASURE found/2.
**Ashley** (stages 5, 6, 7 and 8): the game's model and animations (`ch_ash`, read from NitroFS only for those stages; her skeleton's last 4 bones folded in, to fit the DS's 28). She keeps to Leon's left, a little behind, walking or running to catch up; when Leon aims her way she ducks (shots pass over her). Ganados go for her when she is the nearer; their swings hurt her, and so does a shot that hits her. If she goes down: ASHLEY IS DEAD, game over.

**Stages 2-12** (simple versions, all open in the stage select): each stage's own map and collision (`3d_stgNN_00`, `stg_data_NN.bin`) and haze colour, with Ganados on the game's own enemy points for that stage (`mis_data_sNN.bin`, Ganado kinds only; they all head for Leon). Leon starts on one of those points; the stage clears when all are down. Their bosses, Ashley and the other enemy kinds are not made yet; stage 12's points sit 468, 988 off its model and are moved onto it. The stage models and ground are in NitroFS (`ds/nitrofs`, loaded when a stage is chosen), not in RAM.

Stage 1 is the game's first narrative-mode mission (`mis_data_s01`); its results go back to the stage select. All on-screen text is in English:

1. **The village**: from the fenced path inside the south gate, down every villager (10, at most 5 at once, coming as a group from the open ground ahead). Then, as in the game, **the chainsaw man** (`ch_cha`, with his own walk and chainsaw swing from `ch_cha.mcas`) comes out of a house by the square: 40 health (the handgun barely hurts him; a head shot dazes him for a kick), faster than the villagers, and his chainsaw kills Leon in one blow. The bell rings when he falls or after 90 s.

Each mission starts with its title and objective and ends with MISSION COMPLETE: accuracy, life left, kills, head shots, a total rank and 5000 PTAS. A goes back to the stage select. The village gates stay locked, as in the game ("this door is firmly locked").

Stage 1 gimmicks, placed from the game's own stage data (`stg_data_01.bin`, `mis_data_s01.bin`,
coordinates x256):

- The three village gates (iron gates of the stage model) stay locked.
- **Ladders** up to the walkways (A at the foot climbs up, A at the top edge climbs down); walking
  off a walkway or ledge drops Leon to the ground below.
- **Crates** (kick with A, or shoot), at the stage's action points a10/a11: may hold ammo or a herb.
- As in the Zeebo version's stage 1 (checked against a full playthrough video), there are no
  explosive barrels and no blue medallions. The code for them stays in `objects.c` for later stages.
- **Ganado weapons** (as in the Zeebo version's village): most carry a sickle, a hatchet or a pitchfork (`eqp_wep_all` #14, #10, #11, in the right hand) and walk and swing with the game's weapon animations (`ch_gna_w00`: one-handed, `ch_gna_w01`: pitchfork, which reaches further). A weapon hit costs 25 health (bare hands 15); a quarter are bare-handed and grab.
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
       -DsAnim 'leon_anim=3d_base/ch_leon_base.mcas#0,1,2,11,20,21,13,3,10,8,16', 'leon_gun=3d_base/ch_leon_w01.mcas#0,1,2,8,9,4,6,5,7,3', 'ganado_anim=3d_base/ch_gna_base.mcas#6,7,12,21,9,8,10' `
       -DsGround 'stage_ground=stage+3d_stg01/stg_data_01.bin' `
       -Ds2D 'title=file:assets/title/title_ds_edition_source.png@0,0,1109,832>256,192', 'hud=common/2d_gms_00.mct@128,0,128,128', `
             'stagesel=common/2d_stage_00.mct@0,0,256,224', 'result=common/2d_gm_00.mct@0,48,256,192', `
             'items=common/2d_itemic_00.mct@0,0,128,72!cut32x24|common/2d_itemic_00.mct@160,120,96,24!cut32x24', 'star=3d_base/fix_all.mcts#0@0,0,64,64!glow', `
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
