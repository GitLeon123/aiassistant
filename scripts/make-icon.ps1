# Renders resources/app.ico and the Settings logo (ui/src/assets/logo-96.png)
# from the master artwork in assets/logo.png. Run with Windows PowerShell 5.1+:
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/make-icon.ps1
param(
  [string]$Source = (Join-Path $PSScriptRoot '..\assets\logo.png'),
  [string]$Out = (Join-Path $PSScriptRoot '..\resources\app.ico'),
  [string]$UiOut = (Join-Path $PSScriptRoot '..\ui\src\assets\logo-96.png')
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

function Resize([System.Drawing.Bitmap]$Image, [int]$Size) {
  $bmp = New-Object System.Drawing.Bitmap $Size, $Size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'
  $g.PixelOffsetMode = 'HighQuality'
  $g.CompositingQuality = 'HighQuality'
  $g.Clear([System.Drawing.Color]::Transparent)
  $attrs = New-Object System.Drawing.Imaging.ImageAttributes
  $attrs.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
  $g.DrawImage($Image, (New-Object System.Drawing.Rectangle 0, 0, $Size, $Size), 0, 0, $Image.Width, $Image.Height, 'Pixel', $attrs)
  $g.Dispose()
  return $bmp
}

# Halve repeatedly before the final step so small sizes stay crisp.
function Render([System.Drawing.Bitmap]$Master, [int]$Size) {
  $current = $Master
  while ($current.Width / 2 -ge $Size * 2) {
    $next = Resize $current ([int]($current.Width / 2))
    if ($current -ne $Master) { $current.Dispose() }
    $current = $next
  }
  $result = Resize $current $Size
  if ($current -ne $Master) { $current.Dispose() }
  return $result
}

function PngBytes([System.Drawing.Bitmap]$Bitmap) {
  $ms = New-Object System.IO.MemoryStream
  $Bitmap.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
  return , $ms.ToArray()
}

$master = New-Object System.Drawing.Bitmap (Resolve-Path -LiteralPath $Source).Path
$pngs = @()
foreach ($s in 16, 20, 24, 32, 40, 48, 64, 128, 256) {
  $bmp = Render $master $s
  $pngs += , @($s, (PngBytes $bmp))
  $bmp.Dispose()
}

$uiDir = Split-Path $UiOut
New-Item -ItemType Directory -Path $uiDir -Force | Out-Null
$ui = Render $master 96
[System.IO.File]::WriteAllBytes((Join-Path (Resolve-Path -LiteralPath $uiDir).Path (Split-Path $UiOut -Leaf)), (PngBytes $ui))
$ui.Dispose()
$master.Dispose()

$fs = [System.IO.File]::Create((Resolve-Path -LiteralPath (Split-Path $Out)).Path + '\' + (Split-Path $Out -Leaf))
$bw = New-Object System.IO.BinaryWriter $fs
$bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$pngs.Count)
$offset = 6 + 16 * $pngs.Count
foreach ($p in $pngs) {
  $s = $p[0]; $data = $p[1]
  $dim = if ($s -ge 256) { 0 } else { $s }
  $bw.Write([byte]$dim); $bw.Write([byte]$dim); $bw.Write([byte]0); $bw.Write([byte]0)
  $bw.Write([UInt16]1); $bw.Write([UInt16]32)
  $bw.Write([UInt32]$data.Length); $bw.Write([UInt32]$offset)
  $offset += $data.Length
}
foreach ($p in $pngs) { $bw.Write([byte[]]$p[1]) }
$bw.Close()
Write-Host "Wrote $Out and $UiOut"
