// Text drawn with the game's own font (2d_fnt_00.mct) as main-screen sprites, over the 3D
// picture. The font export keeps only printable ASCII and the characters used in this file,
// so re-run it (see the Makefile) after adding characters. All the text is in English.
#ifndef TEXT_H__
#define TEXT_H__

#include <nds.h>

#define TEXT_LINES      20   // lines on screen at once: 0..11 up to 256 pixels wide,
#define TEXT_WIDE_LINES 12   // 12..19 up to 128
#define TEXT_HEIGHT     14

enum { TEXT_WHITE, TEXT_GOLD, TEXT_RED, TEXT_GREY, TEXT_GREEN, TEXT_PINK };

// Menu strings.
#define T_NEW_GAME   "NEW GAME"
#define T_HOWTO      "CONTROLS"
#define T_RESUME     "RESUME"
#define T_RESTART    "RESTART"
#define T_TO_TITLE   "QUIT TO TITLE"
#define T_CONTINUE   "CONTINUE"
#define T_PAUSE      "PAUSE"
// The status screen's weapon list (the game's Status / Armas, Equipar, Usar, Descartar, Cancelar)
#define T_WEAPONS    "Weapons"
#define T_ST_EQUIP   "Equip"
#define T_ST_USE     "Use"
#define T_ST_DISCARD "Discard"
#define T_ST_CANCEL  "Cancel"
// The pause menu: the game's Opções screen (Bio4_StringLayout.bin), in English
#define T_OPTIONS    "Options"
#define T_OPT_RETRY  "Retry"
#define T_OPT_QUIT   "Quit"
#define T_OPT_SOUND  "Sound"
#define T_OPT_CONTROLS "Controls"
#define T_HELP_RETRY "Retry the mission"
#define T_HELP_QUIT  "Return to the title screen"
#define T_HELP_LEAVE "Return to the stage select"
#define T_HELP_SOUND "Sound settings"
#define T_HELP_CONTROLS "Control settings"
#define T_ASK_RETRY  "Retry?"
#define T_ASK_QUIT   "Return to the title screen?"
#define T_ASK_LEAVE  "Return to the stage select?"
#define T_YES        "YES"
#define T_NO         "NO"
#define T_VOL_OFF    "OFF"
#define T_VOL_LOW    "Volume: Low"
#define T_VOL_MID    "Volume: Medium"
#define T_VOL_HIGH   "Volume: High"
#define T_DEAD       "YOU ARE DEAD"
#define T_ASH_DEAD   "ASHLEY IS DEAD"
#define T_BACK       "B: BACK"
#define T_SELECT     "A: SELECT"
// Stage select
#define T_STAGE_SELECT "STAGE SELECT"
#define T_STAGE_N      "STAGE %d  %s"   // number, place
#define T_SN_1  "VILLAGE"
#define T_SN_2  "VALLEY"
#define T_SN_3  "WATERFALL"
#define T_SN_4  "MINE"
#define T_SN_5  "CHURCH"
#define T_SN_6  "CABIN"
#define T_SN_7  "BATTLEMENTS"
#define T_SN_8  "AUDIENCE HALL"
#define T_SN_9  "TWIN GUARDIANS"
#define T_SN_10 "CENTRAL HALL"
#define T_SN_11 "FURNACE"
#define T_SN_12 "OLD CANAL"
// Item pickups
#define T_PICK_AMMO    "Multi Ammo +10"
#define T_PICK_HERB    "Got a Green Herb"
#define T_PICK_TREASURE "Treasure! +3000 PTAS"
#define T_PICK_MONEY   "Pesetas +%d PTAS"
#define T_TREASURE     "TREASURE"
// Stage gimmicks: action prompts (A when not aiming), medallions
#define T_ACT_GATE     "The gate is locked tight."
#define T_ACT_KICK     "A: Kick"
#define T_ACT_STRUGGLE "Mash A: Struggle!"
#define T_ACT_CUT      "A: Climb"
#define T_ACT_JUMP     "A: Jump"
#define T_GIANT_CLIMB  "Leon reaches the top of"      // (mis_data_s11's lines, in English)
#define T_GIANT_CLIMB_2 "El Gigante from behind."
#define T_GIANT_CUT    "Leon cuts the tentacle coming"
#define T_GIANT_CUT_2  "out of El Gigante's back!"
#define T_ACT_CRATE    "A: Break"
#define T_ACT_DOOR     "A: Open"
#define T_ACT_UP       "A: Climb up"
#define T_ACT_DOWN     "A: Climb down"
#define T_MEDAL        "Blue Medallions %d/%d"
#define T_MEDAL_ALL    "All medallions shot! Ammo +%d"
// The mission (the game's narrative mode, mis_data_s01)
#define T_MISSION      "MISSION %d"
#define T_OBJ_1        "Defeat all the villagers."
#define T_OBJ_N        "Defeat all the enemies."
#define T_ELEV_WAIT    "The elevator takes about 4 minutes."
#define T_ELEV_HERE    "The elevator has arrived."
#define T_BOSS_1       "With the roar of an engine,"
#define T_BOSS_1B      "a man with a chainsaw appears."
#define T_BOSS_2       "Leon: \"Great. A chainsaw.\""
#define T_COMPLETE     "MISSION COMPLETE"
#define T_LIFE         "LIFE"
#define T_KILLS        "KILLS"
#define T_TOTAL        "TOTAL"
#define T_PTAS         "%d PTAS"
// Bottom screen outside the game
#define T_CAPTION      "RESIDENT EVIL 4"
#define T_EDITION      "DS Edition Ver1.0"
// Item select / vital screen
#define T_ITEMS        "ITEM SELECT"
#define T_ITEM_GUN     "Handgun"
#define T_ITEM_MAGNUM  "Magnum"
#define T_ITEM_SHOTGUN "Shotgun"
#define T_ITEM_RIFLE   "Rifle"
#define T_ITEM_TMP     "TMP"
#define T_ITEM_ROCKET  "Rocket Launcher"
#define T_ITEM_AMMO    "Multi Ammo"      // the rounds carried for the gun in hand (pickups go to it)
#define T_ITEM_ROUNDS  "Ammo"
#define T_ITEM_HERB    "Green Herb"
#define T_DESC_GUN     "Loaded %d/%d"
#define T_DESC_AMMO    "Carrying %d rounds"
#define T_DESC_HERB    "Heals %d  Carrying %d"
#define T_HP           "Life %d/%d"
#define T_FULL         "Life is already full."
#define T_TOUCH_HINT   "Tap: select  Double tap: use"
#define T_USED         "Life restored."
#define T_NONE_LEFT    "None left."
#define T_SPARE        "AMMO"
// The shop before a mission (the game's Iniciar, Comprar, Vender, Upgrade; Bio4_StringLayout.bin)
#define T_MISSION_SHOP "Merchant"
#define T_SHOP_CASE_NAME "Larger Case"
#define T_SHOP_START   "Start"
#define T_SHOP_BUY     "Buy"
#define T_SHOP_SELL    "Sell"
#define T_SHOP_UPGRADE "Upgrade"
#define T_SHOP_LINE_1  "Found something interesting?"
#define T_SHOP_LINE_2  "Sell me your items!"
#define T_SHOP_LINE_3  "Going to buy something?"
#define T_SHOP_MONEY   "You don't have enough money!"
#define T_SHOP_SPACE   "You don't have enough space!"
#define T_SHOP_MAX     "That's as far as it goes."
#define T_SHOP_OWNED   "Owned"
#define T_SHOP_EMPTY   "Nothing here, stranger."
#define T_SHOP_PRICE   "%d PTAS"
#define T_SHOP_BOX     "x%d  %d PTAS"
#define T_SHOP_CASE    "Case %d  %d PTAS"
#define T_SHOP_NEXT    "%s > %s  %d"
#define T_SHOP_TOP     "%s  MAX"
#define T_SHOP_LEVELS  "Upgrades %d"
#define T_SHOP_AMMO    "%s Ammo"
#define T_STAT_POWER   "Firepower"
#define T_STAT_CAPACITY "Capacity"
#define T_STAT_RATE    "Firing Speed"
#define T_STAT_RELOAD  "Reload Speed"
// Stage clear
#define T_CLEAR        "STAGE CLEAR"
#define T_ENEMIES      "ENEMIES"
#define T_ACCURACY     "ACCURACY"
#define T_HEADSHOTS    "HEADSHOTS"
#define T_DAMAGE       "DAMAGE TAKEN"
#define T_RANK         "RANK"
#define T_PRESS_A      "PRESS A"
// Controls screen, left column = button, right column = action.
#define T_HOWTO_KEYS \
    "D-Pad", "Move / turn", \
    "B+Up", "Run", \
    "R", "Aim (Up/Down to tilt)", \
    "A", "Fire / kick / climb", \
    "L+A", "Knife", \
    "Y", "Reload", \
    "X, L+X", "Draw gun, next weapon", \
    "START", "Pause"   // (8 rows: the controls screen shows no more)

// Loads the RDF1 font and sets up main-screen sprites (VRAM F and G). Returns 0 on success.
int text_init(const void *font, size_t size);

// Width in pixels of s.
int text_width(const char *s);

// Shows line i at (x, y); the glyphs are only redrawn when the string changes.
void text_line(int i, int x, int y, int color, const char *s);
// The same at double size (sprite scaling): 2 x text_width() wide, 28 pixels high. Only wide
// lines (0..TEXT_WIDE_LINES-1), up to 4 sprites of text (128 pixels before scaling).
void text_big(int i, int x, int y, int color, const char *s);
void text_hide(int i);
void text_clear(void);

// Writes the sprites; call right after the vblank wait.
void text_commit(void);

// Draws s into a 16-bit bitmap (RGB15 | bit 15) with a drop shadow, at 1x or 2x size, clipped
// to w x h. Returns the width drawn.
int text_draw16(u16 *dst, int stride, int w, int h, int x, int y, u16 color, int scale, const char *s);

#endif
