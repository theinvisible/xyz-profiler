# Procedurally generates resources/app.ico for xyz-profiler.
#
# Motif: a stylised optical disc (full circle with a hole in the middle,
# plus a reflective highlight arc) on a rounded dark gradient square. Six
# bitmap sizes (16…256) are packed as PNG payloads inside the ICO so the
# OS picks the right one at every UI scale.
#
# Run once after editing. The output is checked into git.

Add-Type -AssemblyName System.Drawing

function New-IconBitmap([int]$size) {
    $bmp = New-Object System.Drawing.Bitmap $size, $size, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $g.Clear([System.Drawing.Color]::Transparent)

    # ---- Rounded-square background with diagonal gradient ----------------
    $radius = [Math]::Max(2, [int]($size * 0.20))
    $d = $radius * 2
    $bgPath = New-Object System.Drawing.Drawing2D.GraphicsPath
    $bgPath.AddArc(0, 0, $d, $d, 180, 90)
    $bgPath.AddArc($size - $d, 0, $d, $d, 270, 90)
    $bgPath.AddArc($size - $d, $size - $d, $d, $d, 0, 90)
    $bgPath.AddArc(0, $size - $d, $d, $d, 90, 90)
    $bgPath.CloseFigure()

    $bgBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
        (New-Object System.Drawing.Point 0, 0), `
        (New-Object System.Drawing.Point $size, $size), `
        ([System.Drawing.Color]::FromArgb(255, 36, 41, 51)), `
        ([System.Drawing.Color]::FromArgb(255, 18, 21, 27))
    $g.FillPath($bgBrush, $bgPath)
    $bgBrush.Dispose()

    if ($size -ge 32) {
        $strokePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(255, 50, 55, 66)), ([float]([Math]::Max(1, $size / 128.0)))
        $g.DrawPath($strokePen, $bgPath)
        $strokePen.Dispose()
    }
    $bgPath.Dispose()

    # ---- Disc body -------------------------------------------------------
    $padding = [int]($size * 0.18)
    $discSize = $size - 2 * $padding
    $discRect = New-Object System.Drawing.Rectangle $padding, $padding, $discSize, $discSize

    # Radial-ish disc colour — flat blue-grey with a brighter inner ring.
    $discBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
        (New-Object System.Drawing.Point $padding, $padding), `
        (New-Object System.Drawing.Point ($padding + $discSize), ($padding + $discSize)), `
        ([System.Drawing.Color]::FromArgb(255, 96, 165, 250)), `
        ([System.Drawing.Color]::FromArgb(255, 58, 123, 213))
    $g.FillEllipse($discBrush, $discRect)
    $discBrush.Dispose()

    # ---- Inner ring (data area boundary, decorative) ---------------------
    if ($size -ge 24) {
        $innerInset = [int]($discSize * 0.18)
        $innerRect = New-Object System.Drawing.Rectangle ($padding + $innerInset), ($padding + $innerInset), `
                                                          ($discSize - 2 * $innerInset), ($discSize - 2 * $innerInset)
        $ringPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(120, 255, 255, 255)), ([float]([Math]::Max(1, $size / 96.0)))
        $g.DrawEllipse($ringPen, $innerRect)
        $ringPen.Dispose()
    }

    # ---- Specular highlight arc (sheen) ----------------------------------
    if ($size -ge 32) {
        $sheenInset = [int]($discSize * 0.06)
        $sheenRect = New-Object System.Drawing.Rectangle ($padding + $sheenInset), ($padding + $sheenInset), `
                                                          ($discSize - 2 * $sheenInset), ($discSize - 2 * $sheenInset)
        $sheenPen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(110, 255, 255, 255)), ([float]([Math]::Max(1, $size / 56.0)))
        $sheenPen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
        $sheenPen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round
        $g.DrawArc($sheenPen, $sheenRect, 210, 60)
        $sheenPen.Dispose()
    }

    # ---- Center hole -----------------------------------------------------
    $holeSize = [Math]::Max(2, [int]($discSize * 0.22))
    $holeX = [int]($padding + ($discSize - $holeSize) / 2)
    $holeY = [int]($padding + ($discSize - $holeSize) / 2)
    $holeRect = New-Object System.Drawing.Rectangle $holeX, $holeY, $holeSize, $holeSize
    $holeBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 18, 21, 27))
    $g.FillEllipse($holeBrush, $holeRect)
    $holeBrush.Dispose()

    if ($size -ge 24) {
        $holeStrokePen = New-Object System.Drawing.Pen ([System.Drawing.Color]::FromArgb(180, 36, 41, 51)), ([float]([Math]::Max(1, $size / 128.0)))
        $g.DrawEllipse($holeStrokePen, $holeRect)
        $holeStrokePen.Dispose()
    }

    $g.Dispose()
    return $bmp
}

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$pngBuffers = @{}
foreach ($s in $sizes) {
    $bmp = New-IconBitmap $s
    $ms = New-Object System.IO.MemoryStream
    $bmp.Save($ms, [System.Drawing.Imaging.ImageFormat]::Png)
    $pngBuffers[$s] = $ms.ToArray()
    $ms.Dispose()
    $bmp.Dispose()
}

# ---- Pack ICO container with PNG payloads --------------------------------
$icoStream = New-Object System.IO.MemoryStream
$writer = New-Object System.IO.BinaryWriter $icoStream

$writer.Write([UInt16]0)
$writer.Write([UInt16]1)
$writer.Write([UInt16]$sizes.Count)

$headerSize = 6 + 16 * $sizes.Count
$offset = $headerSize
foreach ($s in $sizes) {
    $bytes = $pngBuffers[$s]
    $w = if ($s -ge 256) { [byte]0 } else { [byte]$s }
    $writer.Write([byte]$w)
    $writer.Write([byte]$w)
    $writer.Write([byte]0)
    $writer.Write([byte]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]32)
    $writer.Write([UInt32]$bytes.Length)
    $writer.Write([UInt32]$offset)
    $offset += $bytes.Length
}
foreach ($s in $sizes) {
    $writer.Write($pngBuffers[$s])
}

$icoBytes = $icoStream.ToArray()
$writer.Dispose()
$icoStream.Dispose()

$out = Join-Path $PSScriptRoot "app.ico"
[System.IO.File]::WriteAllBytes($out, $icoBytes)
Write-Output "Wrote $out ($($icoBytes.Length) bytes, $($sizes.Count) sizes)"
