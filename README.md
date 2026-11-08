

# RE4 Zeebo Edition for Nintendo DS（ベータ版）

Zeebo 版『バイオハザード4』（Resident Evil 4 Zeebo Edition、2009年）のデータを使い、
**ニンテンドーDS / DSi のネイティブプログラムとして作り直した** 非公式の移植です。
エミュレーションは使わず、ゲームの処理は C で一から書いています。
解析できなかったデータなどは、プレイ動画を参考にして書き直しました。

> 非公式のファンプロジェクトで、カプコンとは関係ありません。
> ゲームのデータはこのリポジトリに含まれていません。各自が持っている Zeebo 版のデータからビルドしてください。
> ビルドしてできたファイル（`ds/re4ds.nds`、`ds/data/`、`ds/nitrofs/`）はゲームのデータを変換したものを含むため、配布しないでください。
> ベータ版です。動作が安定しないシーンがあります。ご了承ください。

## 仕組み

Zeebo 版のアーカイブから、モデル、アニメーション、テクスチャ、当たり判定、ミッションの配置、
効果音、音楽を PC のツールで DS 用の形式に変換し、それを読み込む DS のプログラムを
[BlocksDS](https://blocksds.skylyrac.net/) でビルドしています。

- `re4zeebo/`：PC 側の変換ツール（C# を PowerShell 7 から実行）
- `ds/source/`：DS 側のゲーム本体（C）

3D は DS のジオメトリエンジンで描き、上画面がゲーム画面、下画面が原作どおりのバイタル／アイテム画面（タッチ操作）です。
MIDI の音楽は DS では鳴らせないため、変換時に Windows の MIDI 音源で録音して DS に入れています。

## 現在の状態

全ステージ、クリアできます。

| 項目 | 状態 |
| --- | --- |
| 動作確認 | melonDS、DS 実機 |
| ステージ | 11ステージ（下の表） |
| 武器 | ハンドガン、ショットガン、ライフル（スコープ付き）、TMP、マグナム、ロケットランチャー、ナイフ |
| ボス | チェーンソー男、エルヒガンテ、ガラドール、ベルデューゴ |
| 画面の文字 | 英語 |
| 物語のシーン | 各ステージの始まりと終わり、ボス戦などで、絵と英語の台詞が出ます（下の「物語のシーン」） |
| セーブ | お金、武器、改造、弾、ハーブをカードの `re4ds.sav` に保存（DLDI が使える環境のみ） |

### ステージ

原作のミッション 6「小屋の籠城戦」は外し（今後のアップデートで追加される可能性は低いです。）、それ以降を詰めて番号を振っています。

| ステージ | 場所 | 内容 |
| --- | --- | --- |
| 1 | VILLAGE（村） | ガナード、チェーンソー男 |
| 2 | VALLEY（谷） | ガナード、ダイナマイトを投げる敵 |
| 3 | WATERFALL（滝） | ガナード |
| 4 | MINE（鉱山） | エルヒガンテ |
| 5 | CHURCH（教会） | アシュリー救出、シャンデリアで向こう側へ |
| 6 | BATTLEMENTS（城壁） | 教徒、燃える石を投げる敵 |
| 7 | AUDIENCE HALL（謁見の間） | ガラドール、吊るされた爆発物 |
| 8 | TWIN GUARDIANS | ガラドール2体、鐘 |
| 9 | CENTRAL HALL（中央ホール） | 教徒 |
| 10 | FURNACE（溶鉱炉） | エルヒガンテ2体 |
| 11 | OLD CANAL（旧水路） | ベルデューゴ、液体窒素のボンベ、エレベーターが来るまで約4分 |

ショップ（購入・売却・改造）は、ステージセレクトとミッションの間にあります。

### 既知の問題

- 未実装：6面の大砲、手榴弾や救急スプレーなどのアイテム、商人の姿、銃の可動部と銃ごとの発射音、スコープのズーム
- 敵が出てくるタイミング（ウェーブ）は原作の処理を解析できておらず、推測で作っています
- 全ステージ分のデータ変換は、まだ1本のスクリプトにまとまっていません
- テキスト表示にバグがあります。（原因不明）
- 小屋の最適化は、DSのハード特性上不可能です。

## 必要なもの

**遊ぶ**

- DS / DS Lite とフラッシュカート、または DSi / 3DS と TWiLight Menu++、またはエミュレーターの melonDS

**ビルドする**

- Windows 10 / 11（音楽の変換に Windows の MIDI 音源を使うため、Windows のみです）
- 下の表のソフト
- 自分で用意した『Resident Evil 4 - Zeebo Edition』のデータ（zip）
- タイトル画面の画像（原作の画像のため含めていません。下の「3. ゲームのデータとタイトル画像を用意する」）

| ソフト | 何に使うか | 入手先 |
| --- | --- | --- |
| PowerShell 7 | 変換ツールとビルドスクリプトを動かす | [インストール手順（Microsoft）](https://learn.microsoft.com/powershell/scripting/install/installing-powershell-on-windows)、[GitHub のリリース](https://github.com/PowerShell/PowerShell/releases) |
| WSL 2 | Docker Desktop が中で使う | [WSL のインストール（Microsoft）](https://learn.microsoft.com/windows/wsl/install) |
| Docker Desktop | DS 用のコンパイラ（BlocksDS）を動かす | [Docker Desktop](https://www.docker.com/products/docker-desktop/)、[Windows へのインストール手順](https://docs.docker.com/desktop/setup/install/windows-install/) |
| BlocksDS | DS 用の SDK とコンパイラ。**自分で入れる必要はありません**（ビルドのときに Docker が [skylyrac/blocksds](https://hub.docker.com/r/skylyrac/blocksds) の `slim-v1.24.0` を自動で取ってきます） | [BlocksDS](https://blocksds.skylyrac.net/)、[GitHub](https://github.com/blocksds/sdk) |
| Git（任意） | リポジトリを取ってくる。ZIP でダウンロードするなら不要 | [Git for Windows](https://git-scm.com/downloads/win) |
| melonDS（任意） | PC で動作を確かめる | [melonDS](https://melonds.kuribo64.net/) |

C# の部分は PowerShell 7 が実行のたびにコンパイルするので、.NET SDK や Visual Studio は要りません。

## ビルド

Windows 10 / 11 での手順です。コマンドは、特に書いていなければ **PowerShell 7** の画面で、リポジトリのフォルダに移動してから実行します。

### 1. 必要なソフトを入れる

**PowerShell 7**

上の表の「インストール手順」から MSI をダウンロードして入れるか、Windows の「ターミナル」で次を実行します。

```powershell
winget install --id Microsoft.PowerShell --source winget
```

Windows に最初から入っている「Windows PowerShell」（青い画面、バージョン 5.1）では動きません。
スタートメニューの「PowerShell 7」を開くか、ターミナルで `pwsh` と入力して切り替えてください。
`$PSVersionTable.PSVersion` で 7 以上と出れば大丈夫です。

**WSL 2 と Docker Desktop**

1. 管理者として開いたターミナルで `wsl --install` を実行し、PC を再起動します（すでに入っていれば不要）。
2. [Docker Desktop](https://www.docker.com/products/docker-desktop/) をダウンロードして入れます。途中で「Use WSL 2 instead of Hyper-V」を選んでください。
3. Docker Desktop を起動し、左下が「Engine running」になるまで待ちます。
4. 次のコマンドで、メッセージ（Hello from Docker!）が出れば準備完了です。

   ```powershell
   docker run --rm hello-world
   ```

Docker Desktop は、ビルドするときに毎回起動しておく必要があります。

**Git（任意）**

[Git for Windows](https://git-scm.com/downloads/win) を入れます。設定は初期値のままで構いません。

### 2. リポジトリを取ってくる

```powershell
git clone <このリポジトリの URL>
cd <リポジトリのフォルダ>
```

Git を使わない場合は、GitHub のページの「Code」→「Download ZIP」でダウンロードして展開します。
その場合は、展開したフォルダで次を一度だけ実行してください（インターネットから来たファイルとしてスクリプトが止められるのを解除します）。

```powershell
Get-ChildItem -Recurse | Unblock-File
```

「このシステムではスクリプトの実行が無効になっている」と出る場合は、その画面で次を実行してから続けます（その画面を閉じるまで有効です）。

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
```

### 3. ゲームのデータとタイトル画像を用意する

- **ゲームのデータ**：『Resident Evil 4 - Zeebo Edition』の zip を、どこか分かる場所に置きます（展開しなくて構いません）。
  中に `.h2z` のアーカイブが入った zip です。展開済みのフォルダや、`.h2z` が入ったフォルダを指定することもできます。
- **タイトル画像**：`assets/title/title_ds_edition_source.png` に PNG を置きます。
  左上の 1109 × 832 ピクセルを切り出して 256 × 192 に縮めて使うので、**1109 × 832 ピクセル以上**の画像にしてください（4:3 の画像なら 1109 × 832 ちょうどにするときれいに収まります）。
  フォルダが無ければ作ります。

  ```powershell
  New-Item -ItemType Directory -Force assets/title
  ```

### 4. ゲームのデータを DS 用に変換する

```powershell
./re4zeebo/export_all.ps1 -InputPath "D:\Downloads\Resident Evil 4 - Zeebo Edition.zip"
```

`-InputPath` は自分の zip の場所に書き換えてください。スペースを含むので `"` で囲みます。
これ1本で、次のことを順に行います。全部で、音楽の録音を除いて2分ほど（手元の PC で約1分半）、録音に1分半ほどかかります。

| 順番 | 内容 | できるもの |
| --- | --- | --- |
| 1 | zip の展開（2回目からは省略） | `re4zeebo/out/files/` ほか |
| 2 | レオン、ガナード、仕掛け、武器、2D 画像、文字、効果音 | `ds/data/*.bin`（ROM に入ります） |
| 3 | 音楽の録音（下の注意） | `ds/data/music.bin` |
| 4 | 12 ステージのモデルと当たり判定、アシュリーやボス、ほかの銃 | `ds/nitrofs/*.bin`（遊んでいる間に読み込みます） |
| 5 | 敵の配置、ショップの改造表、物語のシーン（とその絵） | `ds/source/stage_foes.c`、`shop_tune.c`、`cutins.c`、`ds/nitrofs/eve*.bin` |

**音楽の録音についての注意**：DS では MIDI を鳴らせないので、原作の MIDI を Windows の MIDI 音源（Microsoft GS Wavetable Synth）で実際に鳴らし、PC の音声出力をそのまま録音しています。

- 録音中（約1分半）は、ほかの音（動画、通知音など）を鳴らさないでください。一緒に録音されます。
- スピーカーやヘッドホンなど、音の出力先が有効になっている必要があります。
- 録音の結果は `re4zeebo/out/music/*.wav` にも保存されるので、聞いて確かめられます。
- 音楽だけ後で録り直すには、次のようにします。

  ```powershell
  ./re4zeebo/export_all.ps1 -InputPath "<zip の場所>" -NoMusic     # 音楽以外を変換
  ./re4zeebo/export_all.ps1 -InputPath "<zip の場所>" -OnlyMusic   # 音楽だけ録音
  ```

最後に `Done.` と出れば完了です。途中で止まった場合は、赤いエラーの直前に `== ` で始まる行があり、どの変換で止まったかが分かります。

### 5. ビルドする

Docker Desktop が起動している（Engine running）ことを確かめてから、次を実行します。

```powershell
./build.ps1
```

`ds/re4ds.nds` ができれば成功です。

- 初回は BlocksDS のイメージ（展開後 約 1 GB）をダウンロードするので時間がかかります。2回目からは1分ほどで終わります。
- 中間ファイルを消して最初からビルドし直すには `./build.ps1 clean` のあと `./build.ps1` です。
- ソースだけを直したときは、`./build.ps1` だけで構いません（4. の変換はやり直さなくて大丈夫です）。

### 6. PC で動かしてみる（任意）

[melonDS](https://melonds.kuribo64.net/) をダウンロードして、`tools/melonDS/melonDS.exe` になるように展開しておくと、ビルドしてすぐ起動できます。

```powershell
./build.ps1 -Run
```

melonDS を別の場所に置いた場合は、melonDS の「File」→「Open ROM」から `ds/re4ds.nds` を開きます。
melonDS の操作のキー割り当ては「Config」→「Input and hotkeys」で変えられます。マウスが下画面のタッチです。

### 一部だけ作り直す

| 何を変えたか | やること |
| --- | --- |
| `ds/source/` の C のソース | `./build.ps1` |
| `ds/source/text.h` の文字（新しい文字を使った） | 下の文字の変換をしてから `./build.ps1` |
| `re4zeebo/cutins_en.txt`（台詞の英訳） | `./re4zeebo/gen_cutins.ps1` のあと `./build.ps1` |
| 変換ツール（`re4zeebo/*.cs`） | `./re4zeebo/export_all.ps1 -InputPath "<zip の場所>" -NoMusic` のあと `./build.ps1` |

文字の変換（`text.h` で使っている文字だけを、ゲームのフォントから取り出します）：

```powershell
./re4zeebo/re4zeebo.ps1 -InputPath re4zeebo/out/files -OutDir ds/data -DsFont 'font=common/2d_fnt_00.mct;common/2d_fnt.hfi;ds/source/text.h'
```

`export_all.ps1` の中身は、変換1回ごとに1行ずつ書いてあります。ほかの部分だけ作り直したいときは、その行の引数を `re4zeebo.ps1` に渡してください。
引数の書き方（`-DsModel`、`-DsAnim`、`-Ds2D` など）は `re4zeebo/re4zeebo.ps1` の先頭と [docs/NOTES.md](docs/NOTES.md) の Building にあります。

### うまくいかないとき

| 出たメッセージ・症状 | 原因と対処 |
| --- | --- |
| `failed to connect to the docker API`、`docker: The term 'docker' is not recognized` | Docker Desktop が起動していません。起動して「Engine running」になるまで待ちます |
| `スクリプトの実行が無効` / `running scripts is disabled` | 「2. リポジトリを取ってくる」の `Set-ExecutionPolicy` を実行します |
| `Add-Type` や C# のエラーがたくさん出る | Windows PowerShell 5.1 で実行しています。PowerShell 7（`pwsh`）で実行し直します |
| `assets/title/title_ds_edition_source.png is missing` | タイトル画像がありません。「3.」のとおり置いてから、4. をやり直します |
| ビルドで `title_bin` などが見つからないというエラー | `ds/data` のファイルが足りません。4. が最後（`Done.`）まで終わっているか確かめます |
| 音楽が鳴らない、雑音が入っている | 録音中にほかの音が鳴ったか、出力先が無効でした。`-OnlyMusic` で録り直します |
| 物語のシーンで絵が出ず、台詞だけ出る | `ds/nitrofs/eve*.bin` がありません。`./re4zeebo/gen_cutins.ps1` を実行してからビルドします |
| 実機で起動しない | DS のフラッシュカートによっては DLDI のパッチが必要です。フラッシュカートの説明を見てください |



## インストール

できあがった `ds/re4ds.nds` を使います。

- **DS / DS Lite（フラッシュカート）**：SD カードにコピーして、フラッシュカートのメニューから起動
- **DSi / 3DS（TWiLight Menu++）**：`sd:/roms/nds/` などにコピーして起動
- **melonDS**：`melonDS.exe ds/re4ds.nds`

セーブは ROM と同じカードのルートに `re4ds.sav` として作られます。
melonDS で試すときは、DLDI とフォルダの同期を有効にして、エミュレーターを普通に終了してください（終了時に書き込まれます）。

## 操作

| ボタン | 動作 |
| --- | --- |
| 十字キー 上 / 下 | 前進 / 後退 |
| 十字キー 左 / 右 | 向きを変える |
| B + 上 | 走る |
| X | 銃を構える / しまう |
| L + X | 次の武器に持ち替える |
| R（押したまま） | 狙う（上下で狙いの角度、左右でゆっくり向きを変える） |
| A（狙っているとき） | 撃つ |
| A（狙っていないとき） | 蹴る、はしごを上り下り、扉を開ける、ジャンプなど（できることが画面に出ます） |
| L（押したまま）+ A | ナイフ（上下で視点と上半身の角度が変わります） |
| Y | リロード（リロード中も歩けます） |
| A / B / X / Y 連打 | つかまれたときに振りほどく |
| 下画面をタッチ | アイテムを選ぶ、ハーブをダブルタップで使う |
| START | ポーズメニュー |
| SELECT | ステータス画面（武器の装備、アイテムを使う・捨てる） |

メニューでは、上下（ステージセレクトは左右）で選び、A で決定、B で戻ります。

## テスト用のビルド

`-DAUTOPILOT=n` などの定義を付けてビルドすると、自動操作や表示の確認ができます。
一覧は [docs/NOTES.md](docs/NOTES.md) の Test builds にあります。

```powershell
docker run --rm -v "${PWD}/ds:/work" -w /work skylyrac/blocksds:slim-v1.24.0 make -B "DEFINES=-DSKIP_TITLE -DSTART_STAGE=5"
```

確認が終わったら、定義を付けずにビルドし直してください。

## 開発メモ

ゲームのファイル形式の解析結果、原作との違い、各ステージの調整の記録は
[docs/NOTES.md](docs/NOTES.md)（英語・AI翻訳）にまとめています。
マルチプレイにも対応させたいけど、ポリゴン上限が中々シビアで、実現できるか怪しいです。

## 不具合の報告

ステージの番号、場所、何が起きたかを書いて報告してください。実機の写真や動画があると助かります。

## ライセンス

このプロジェクトのコードは [MIT ライセンス](LICENSE) です。

ライセンスは『バイオハザード4』そのものには及びません。
バイオハザード（Resident Evil）および CAPCOM は株式会社カプコンの商標です。
Zeebo は Zeebo Inc. の商標です。

## クレジット

- DS 移植：木々諒（Macoto Kiki）
- [BlocksDS](https://github.com/blocksds/sdk)：DS 用の SDK とコンパイラ
- [melonDS](https://melonds.kuribo64.net/)：動作確認に使ったエミュレーター
