<#
.SYNOPSIS
  RE4 Zeebo Edition asset tool: MCT textures -> PNG, MCM models -> OBJ/MTL, polygon counts.

.EXAMPLE
  ./re4zeebo.ps1 -InputPath "C:\Users\User\Downloads\Resident Evil 4 - Zeebo Edition.zip" -OutDir ./out -Preview

.NOTES
  InputPath may be the Zeebo zip, a folder containing .h2z archives, or a single .h2z.
  Output: textures/<archive>/*.png, models/<archive>/*.obj + .mtl (textures linked),
  textures.csv, meshes.csv, draws.csv, summary.txt.
  -Preview also renders models/<archive>/*.preview.png (front view, textured).
  -Unpack also writes the raw files under files/<archive>/.
  -DsModel switches to DS export: each "name=archive/file.mcms[#part]" becomes <OutDir>/<name>.bin
  (format described in DsExport.cs); -VramBudget caps the total texture size in bytes.
  -DsAnim adds animation files: "name=archive/file.mcas#i,j,k" becomes <OutDir>/<name>.bin.
  -DsGround adds floor-triangle files for height lookups: "name=model" (a -DsModel name).
  -Ds2D writes picture sheets: "name=archive/file.mct@x,y,w,h[>W,H]" (crop, optional resize).
  -DsFont writes the glyphs a text file uses: "name=archive/font.mct;archive/font.hfi;textfile".
  -DsSound writes a sound bank: "name=archive/folder:a,b,c" (WAV names, in this order).
  -DsMusic records MIDI tracks through Windows' MIDI synth (real time, from the audio output:
  nothing else may play meanwhile) into a looping IMA-ADPCM file: "name=archive/folder:a,b"
  (MIDI names); -MusicRate sets its sample rate. The recordings are kept in out/music/*.wav.
  (Formats in Ds2D.cs and DsSound.cs. -Ds2D/-DsFont/-DsSound can run without -DsModel.)
  Requires PowerShell 7+ (the C# part is compiled on the fly).
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
[Text.Encoding]::RegisterProvider([Text.CodePagesEncodingProvider]::Instance)   # Shift-JIS font maps
# Image files as sheet sources ("file:path.png@..."), relative to the current directory.
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
    # Relative text-file paths in -DsFont are taken from the current directory.
    $fonts = @($DsFont | Where-Object { $_ } | ForEach-Object {
        $p = $_ -split ';'
        if ($p.Count -eq 3) { $p[2] = (Resolve-Path -LiteralPath $p[2]).Path }
        $p -join ';' })
    if ($Ds2D -or $DsFont) { $summary += [Re4Zeebo.Tool]::Export2D($in, $out, $Ds2D, $fonts, $log) }
    if ($DsSound) { $summary += [Re4Zeebo.Tool]::ExportSounds($in, $out, $DsSound, $log) }
    # Music is recorded from the PC's audio output in real time: keep everything else quiet.
    if ($DsMusic) { $summary += [Re4Zeebo.Tool]::ExportMusic($in, $out, (Join-Path $PSScriptRoot 'out\music'), $DsMusic, $MusicRate, $log) }
} else {
    $summary = [Re4Zeebo.Tool]::Run($in, $out, $Unpack.IsPresent, $Preview.IsPresent, $log)
}
''
$summary
''
"Output: $out"
