// 下画面：アイテム選択 / バイタル画面、ゲーム中は常に出し、タッチで操作する。原作のケースのパネル
// （2d_bks_01 / 2d_bks_00）、アイテムのアイコン（2d_itemic_00）、ゲームのフォントから、16ビットの
// ビットマップの背景（VRAM C）に描く。
#ifndef VITAL_H__
#define VITAL_H__

#include <nds.h>

#define VITAL_SLOTS 3   // 手に持っている銃、その弾（多機能弾丸）、グリーンハーブ

typedef struct {
    int hp, hp_max;
    int ammo, mag;      // 込めてある弾 / マガジンの大きさ
    int spare;          // 持っている弾
    int herbs, heal;    // 持っているハーブ、1つで戻る体力
    int fps;
    const char *gun_name;   // 手に持っている銃（NULL：ハンドガンのもの）
    int gun_icon[2], ammo_icon[2];   // アイコンのシートでのそのアイコン（2d_itemic_00、アイテムのシートと同じ配置）
    // マップ（既定の表示。ポーズメニューの Bottom Screen のオプションでアイテムと切り替える）：ステージ
    // （0始まり、-1：なし）、レオンのいる所（f32 のワールドの x, z）と向き（libnds の角度）、アシュリー。
    int stage;
    int leon_x, leon_z, leon_yaw;
    int ash_on, ash_x, ash_z;
    int items_view;     // 1：マップの代わりにアイテム選択（状態、心拍、枠）
} Vital;

enum { VITAL_NONE, VITAL_SELECT, VITAL_USE };

// サブ画面を準備する（BG3 に16ビットのビットマップ、VRAM C）。成功で0を返す。
int vital_init(const void *panels, size_t panels_size, const void *icons, size_t icons_size);

// メニュー：背景と、見出しのバーの見出しと、その下の（ASCII の）副題だけ。
void vital_idle(const char *caption, const char *subtitle);

// ゲーム中：ステージのマップ、レオンの動かない矢印のまわりで回して動かす。左に縦に並ぶアイテムの
// ショートカットと表示の帯（vital.c）。タッチを扱う（`touch` があるとき）：ショートカットに触れると
// 選ぶ（VITAL_SELECT）。ハーブのダブルタップで VITAL_USE を返す。
int vital_update(const Vital *v, u32 down, int tick, int touch);
// マップの回転とスプライトを設定する：vblank の待ちのすぐ後に呼ぶ（text_commit() と一緒に）。
void vital_commit(void);

// サブ画面のビットマップにストーリーの場面の1ページ（上に出す：呼び出し側が画面を入れ替えるので、
// テクスチャのメモリは要らない）：上に黒の上の絵（RD2D シート、256 x 128、8ビットのパレット。
// NULL なら無し）、下に話す人（NULL：誰もいない）と3行まで（'\n'）。
// 次の vital_update() / vital_idle() が自分の画面を配置し直す。
void vital_cutin(const void *picture, size_t size, const char *who, const char *text);

// 説明のパネルに `frames` フレームの間出す表示（たとえば「health is full」）。
void vital_message(const char *s, int frames);

// テスト用ビルド：次の vital_update() が (x, y) のタッチを受ける（エミュレーターはスクリプトから
// タッチできない）。
void vital_fake_touch(int x, int y);

#endif
