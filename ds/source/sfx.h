// 原作の効果音（re4zeebo -DsSound の RSN1 のバンク）、libnds のサウンド API を通して DS の
// ハードウェアのチャンネルで再生する。
#ifndef SFX_H__
#define SFX_H__

#include <nds.h>

// バンクの順（re4zeebo -DsSound 'sfx=common/snd:wep15_shot5,fn57_kara,...'）。
enum {
    SFX_SHOT,           // wep15_shot5：ハンドガンの射撃
    SFX_EMPTY,          // fn57_kara：空撃ち、マガジンが空
    SFX_RELOAD,         // wep15_reload_re
    SFX_WALL,           // bogan_kabe：弾が壁に当たる
    SFX_GANADO_HIT,     // v_damege03：村人が傷を負う
    SFX_GANADO_SPOT,    // v_fd1：レオンを見つけた村人の叫び
    SFX_GANADO_ATTACK,  // ikaku01：攻撃を始めるときの叫び
    SFX_SWING,          // kazekiri：腕を振る（空を切る）
    SFX_GANADO_DOWN,    // zea_c_down：倒れる
    SFX_LEON_HURT,      // leon_dam1
    SFX_LEON_HURT2,     // leon_dam2：噛まれる / 倒れて死ぬ
    SFX_BREAK_FREE,     // escape_hit：つかみから蹴って抜ける
    SFX_MENU,           // sub_kettei2：メニューで決定
    SFX_ITEM_GET,       // itemget_fixeq2：アイテムを拾う
    SFX_BELL,           // demo_kane：教会の鐘（ステージクリア）
    SFX_DOOR_KICK,      // door_w01_kick：門を蹴り開ける
    SFX_BREAK,          // tate_break1：木が壊れる（木箱、メダル）
    SFX_EXPLODE,        // handbomb：樽の爆発
    SFX_KNIFE,          // knife：ナイフの斬り
    SFX_COUNT
};

// バンクを解析してサウンドをオンにする。成功で0を返す。
int sfx_init(const void *bank, size_t size);

// 音を再生する（音量 0..127、パン 0 左 .. 64 中央 .. 127 右）。チャンネルを返す。
// -1 なら（音なし）。
int sfx_play(int id, int volume, int pan);
// 全部の音の全体の音量、パーセント（100 = 指定どおり）。
void sfx_level(int percent);

// ワールドの点 `pos`（f32）から来る音を、`ear` にいて `yaw`（libnds の角度）を向いた聞き手に
// 聞こえるように再生する：距離で小さく、その側へパンする。
int sfx_play_at(int id, int volume, const int pos[3], const int ear[3], int yaw);

// sfx_play*() で始めた音を止める（-1 は無視）。
void sfx_stop(int channel);

#endif
