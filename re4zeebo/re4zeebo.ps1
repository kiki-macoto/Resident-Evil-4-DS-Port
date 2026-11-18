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
[Text.Encoding]::RegisterProvider([Text.CodePagesEncodingProvider]::Instance)
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
    $fonts = @($DsFont | Where-Object { $_ } | ForEach-Object {
        $p = $_ -split ';'
        if ($p.Count -eq 3) { $p[2] = (Resolve-Path -LiteralPath $p[2]).Path }
        $p -join ';' })
    if ($Ds2D -or $DsFont) { $summary += [Re4Zeebo.Tool]::Export2D($in, $out, $Ds2D, $fonts, $log) }
    if ($DsSound) { $summary += [Re4Zeebo.Tool]::ExportSounds($in, $out, $DsSound, $log) }
    if ($DsMusic) { $summary += [Re4Zeebo.Tool]::ExportMusic($in, $out, (Join-Path $PSScriptRoot 'out\music'), $DsMusic, $MusicRate, $log) }
} else {
    $summary = [Re4Zeebo.Tool]::Run($in, $out, $Unpack.IsPresent, $Preview.IsPresent, $log)
}
''
$summary
''
"Output: $out"
