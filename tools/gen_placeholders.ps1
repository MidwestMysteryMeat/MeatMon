# Regenerates the committed placeholder art (tileset + demo creature sprites).
# All output is original programmer art — safe to commit and ship.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$root = Split-Path $PSScriptRoot -Parent

function New-Png([int]$w, [int]$h, [string]$relPath, [scriptblock]$draw) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    & $draw $g $bmp
    $g.Dispose()
    $path = Join-Path $root $relPath
    New-Item -ItemType Directory -Force (Split-Path $path) | Out-Null
    $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host "wrote $relPath"
}

# global: so scriptblock closures (GetNewClosure) can resolve it
function global:B([int]$r, [int]$g2, [int]$b) {
    New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, $r, $g2, $b))
}

# --- tileset: 4 tiles in a row, 16x16 each: grass, path, water, tree --------
New-Png 64 16 "game/assets/tilesets/overworld.png" {
    param($g, $bmp)
    # 1: grass
    $g.FillRectangle((B 52 128 66), 0, 0, 16, 16)
    foreach ($p in @(@(2,3),@(6,9),@(11,5),@(13,12),@(4,13),@(9,1),@(14,7),@(1,8))) {
        $bmp.SetPixel($p[0], $p[1], [System.Drawing.Color]::FromArgb(255, 40, 104, 52))
    }
    # 2: path
    $g.FillRectangle((B 198 166 112), 16, 0, 16, 16)
    foreach ($p in @(@(19,4),@(24,10),@(28,6),@(21,13),@(30,2),@(26,14))) {
        $bmp.SetPixel($p[0], $p[1], [System.Drawing.Color]::FromArgb(255, 172, 140, 92))
    }
    # 3: water
    $g.FillRectangle((B 56 96 184), 32, 0, 16, 16)
    $g.FillRectangle((B 96 136 216), 34, 4, 6, 1)
    $g.FillRectangle((B 96 136 216), 40, 10, 6, 1)
    # 4: tree (grass base, canopy, trunk)
    $g.FillRectangle((B 52 128 66), 48, 0, 16, 16)
    $g.FillRectangle((B 92 64 40), 54, 10, 4, 5)
    $g.FillEllipse((B 30 84 44), 49, 0, 14, 12)
    $g.FillEllipse((B 44 108 56), 51, 2, 8, 6)
}

# --- creature placeholders ----------------------------------------------------
function New-Mon([string]$slug, [int]$r, [int]$g2, [int]$b) {
    New-Png 48 48 "game/assets/custom/$slug/front.png" {
        param($g, $bmp)
        $g.FillEllipse((B $r $g2 $b), 8, 12, 32, 30)                       # body
        $g.FillEllipse((B ([Math]::Min(255,$r+40)) ([Math]::Min(255,$g2+40)) ([Math]::Min(255,$b+40))), 12, 6, 24, 20)  # head
        $g.FillEllipse((B 255 255 255), 17, 12, 6, 6)                      # eyes
        $g.FillEllipse((B 255 255 255), 26, 12, 6, 6)
        $g.FillEllipse((B 20 20 20), 19, 14, 3, 3)
        $g.FillEllipse((B 20 20 20), 28, 14, 3, 3)
    }.GetNewClosure()
    New-Png 48 48 "game/assets/custom/$slug/back.png" {
        param($g, $bmp)
        $g.FillEllipse((B ([Math]::Max(0,$r-30)) ([Math]::Max(0,$g2-30)) ([Math]::Max(0,$b-30))), 8, 12, 32, 30)
        $g.FillEllipse((B $r $g2 $b), 12, 6, 24, 20)
    }.GetNewClosure()
}
New-Mon "emberling" 224 96 48    # orange
New-Mon "puddlit"   72 120 216   # blue
New-Mon "sprigling" 88 168 88    # green
New-Mon "zapkin"    228 200 60   # yellow

# --- font atlas: ASCII 32..127 in a 16x6 grid of 8x14 cells -------------------
New-Png 128 84 "game/assets/fonts/mono.png" {
    param($g, $bmp)
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit
    $font = New-Object System.Drawing.Font("Consolas", 12, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
    $fmt = [System.Drawing.StringFormat]::GenericTypographic
    $white = B 255 255 255
    for ($c = 32; $c -lt 128; $c++) {
        $col = ($c - 32) % 16
        $row = [Math]::Floor(($c - 32) / 16)
        $g.DrawString([string][char]$c, $font, $white, $col * 8, $row * 14 + 1, $fmt)
    }
    $font.Dispose()
}

# --- people (16x16): player + NPCs, same body different jacket ----------------
function New-Person([string]$slug, [int]$jr, [int]$jg, [int]$jb) {
    New-Png 16 16 "game/assets/custom/$slug/front.png" {
        param($g, $bmp)
        $g.FillRectangle((B 236 188 148), 5, 2, 6, 5)     # head
        $g.FillRectangle((B $jr $jg $jb), 4, 7, 8, 5)     # jacket
        $g.FillRectangle((B 40 56 88), 5, 12, 2, 3)       # legs
        $g.FillRectangle((B 40 56 88), 9, 12, 2, 3)
        $bmp.SetPixel(6, 4, [System.Drawing.Color]::FromArgb(255, 20, 20, 20))  # eyes
        $bmp.SetPixel(9, 4, [System.Drawing.Color]::FromArgb(255, 20, 20, 20))
    }.GetNewClosure()
}
New-Person "player"   200 48 48    # red
New-Person "rival"    148 72 200   # purple
New-Person "villager" 72 148 88    # green
Write-Host "placeholders regenerated"
