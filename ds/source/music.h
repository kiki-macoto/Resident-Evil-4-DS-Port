// BGM：ループする IMA-ADPCM の曲（re4zeebo -DsMusic の RMU1 ファイル）、それぞれハードウェアの
// 1チャンネルで再生する。先に sfx_init() が必要（それがサウンドをオンにする）。
#ifndef MUSIC_H__
#define MUSIC_H__

#include <nds.h>

// 曲の順（re4zeebo -DsMusic 'music=common/snd:st001,st003'）。
enum { MUSIC_TITLE, MUSIC_VILLAGE, MUSIC_COUNT };

// ファイルを解析する。成功で0を返す。
int music_init(const void *data, size_t size);

// 曲を音量 0..127 でループ再生し始める。もう再生中の曲は音量だけを変える。
void music_play(int track, int volume);

void music_volume(int volume);
int music_asked(void);   // 最後に頼まれた音量（music_play / music_volume）
// 全体の音量、パーセント（ポーズメニューの Sound の設定）。すぐに反映する。
void music_level(int percent);
void music_stop(void);

#endif
