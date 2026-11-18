<#
.SYNOPSIS
RE4 Zeebo Edition の素材のツール：MCT のテクスチャ -> PNG、MCM のモデル -> OBJ/MTL、ポリゴン数。

.EXAMPLE
./re4zeebo.ps1 -InputPath "C:\Users\User\Downloads\Resident Evil 4 - Zeebo Edition.zip" -OutDir ./out -Preview

.NOTES
InputPath は Zeebo の zip、.h2z のアーカイブのあるフォルダー、または1つの .h2z。
出力：textures/<archive>/*.png、models/<archive>/*.obj + .mtl（テクスチャを参照）、
textures.csv、meshes.csv、draws.csv、summary.txt。
-Preview は models/<archive>/*.preview.png も描く（正面から、テクスチャ付き）。
-Unpack は生のファイルも files/<archive>/ の下に書く。
-DsModel で DS の書き出しになる：各「name=archive/file.mcms[#part]」が <OutDir>/<name>.bin になる
（形式は DsExport.cs に記述）。-VramBudget はテクスチャの合計の大きさをバイトで制限する。
-DsAnim はアニメーションのファイルを足す：「name=archive/file.mcas#i,j,k」が <OutDir>/<name>.bin になる。
-DsGround は高さの検索用の床の三角形のファイルを足す：「name=model」（-DsModel の名前）。
-Ds2D は絵のシートを書く：「name=archive/file.mct@x,y,w,h[>W,H]」（切り取り、大きさの変更は任意）。
-DsFont はテキストファイルが使う文字を書く：「name=archive/font.mct;archive/font.hfi;textfile」。
-DsSound はサウンドのバンクを書く：「name=archive/folder:a,b,c」（WAV の名前、この順で）。
-DsMusic は MIDI の曲を Windows の MIDI シンセで録音し（実時間、音声出力から：その間ほかは何も
鳴らしてはいけない）、ループする IMA-ADPCM のファイルにする：「name=archive/folder:a,b」
（MIDI の名前）。-MusicRate でそのサンプリングレートを設定する。録音は out/music/*.wav に残る。
（形式は Ds2D.cs と DsSound.cs に。-Ds2D/-DsFont/-DsSound は -DsModel なしでも実行できる。）
PowerShell 7 以上が必要（C# の部分はその場でコンパイルする）。
.EXAMPLE
./re4zeebo.ps1 -InputPath <zip> -OutDir ../ds/data -DsModel stage=3d_stg01/stg_01.mcms, leon=3d_base/ch_leon.mcms `
-DsAnim leon_anim=3d_base/ch_leon_base.mcas#0,1,2 -DsGround stage_ground=stage
#>
param(
    [Parameter(Mandatory)] [string] $InputPath,
    [string] $OutDir = (Join-Path $PSScriptRoot 'out'),
    [switch] $Unpack,
    [switch] $Preview,
    [string[]] $DsModel,
    [string[]] $DsAnim,
    [string[]] $DsGround,
    [int] $VramBudget = 384KB,
    [string[]] $Ds2D,
    [string[]] $DsFont,
    [string[]] $DsSound,
    [string[]] $DsMusic,
    [int] $MusicRate = 16384
)
$ErrorActionPreference = 'Stop'

if (-not ('Re4Zeebo.Tool' -as [type])) {
    Add-Type -Path (Join-Path $PSScriptRoot 'Re4Zeebo.cs'), (Join-Path $PSScriptRoot 'Mcm.cs'), (Join-Path $PSScriptRoot 'Anim.cs'), (Join-Path $PSScriptRoot 'DsExport.cs'), (Join-Path $PSScriptRoot 'Ds2D.cs'), (Join-Path $PSScriptRoot 'DsSound.cs'), (Join-Path $PSScriptRoot 'DsMusic.cs')
}
[Text.Encoding]::RegisterProvider([Text.CodePagesEncodingProvider]::Instance)   # Shift-JIS のフォントの対応表
# シートのソースとしての画像ファイル（「file:path.png@...」）、今のディレクトリからの相対。
[Re4Zeebo.Tool]::ImageLoader = [Func[string, object[]]] {
    param($p)
    Add-Type -AssemblyName System.Drawing
    $bmp = [System.Drawing.Bitmap]::new((Resolve-Path -LiteralPath $p).Path)
    try {
        $rect = [System.Drawing.Rectangle]::new(0, 0, $bmp.Width, $bmp.Height)
        $data = $bmp.LockBits($rect, 'ReadOnly', [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $bytes = [byte[]]::new($bmp.Width * $bmp.Height * 4)
        for ($y = 0; $y -lt $bmp.Height; $y++) {
            [Runtime.InteropServices.Marshal]::Copy([IntPtr]($data.Scan0.ToInt64() + $y * $data.Stride), $bytes, $y * $bmp.Width * 4, $bmp.Width * 4)
        }
        $bmp.UnlockBits($data)
        return @($bytes, $bmp.Width, $bmp.Height)
    } finally { $bmp.Dispose() }
}

$in = (Resolve-Path -LiteralPath $InputPath).Path
$out = [IO.Path]::GetFullPath($OutDir)
$log = [Action[string]] { param($m) Write-Host $m }
if ($DsModel -or $DsAnim -or $Ds2D -or $DsFont -or $DsSound -or $DsMusic) {
    $summary = @()
    if ($DsModel -or $DsAnim) { $summary += [Re4Zeebo.Tool]::ExportDs($in, $out, [string[]]@($DsModel | Where-Object { $_ }), $DsAnim, $DsGround, $VramBudget, $log) }
    # -DsFont のテキストファイルの相対パスは今のディレクトリから取る。
    $fonts = @($DsFont | Where-Object { $_ } | ForEach-Object {
        $p = $_ -split ';'
        if ($p.Count -eq 3) { $p[2] = (Resolve-Path -LiteralPath $p[2]).Path }
        $p -join ';' })
    if ($Ds2D -or $DsFont) { $summary += [Re4Zeebo.Tool]::Export2D($in, $out, $Ds2D, $fonts, $log) }
    if ($DsSound) { $summary += [Re4Zeebo.Tool]::ExportSounds($in, $out, $DsSound, $log) }
    # 音楽は PC の音声出力から実時間で録音する：ほかの音は鳴らさないこと。
    if ($DsMusic) { $summary += [Re4Zeebo.Tool]::ExportMusic($in, $out, (Join-Path $PSScriptRoot 'out\music'), $DsMusic, $MusicRate, $log) }
} else {
    $summary = [Re4Zeebo.Tool]::Run($in, $out, $Unpack.IsPresent, $Preview.IsPresent, $log)
}
''
$summary
''
"Output: $out"
