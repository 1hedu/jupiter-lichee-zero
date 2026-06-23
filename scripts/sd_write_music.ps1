# Write the WC1 music pack onto a raw SD device at LBA 524288.
#
# Run from an *elevated* PowerShell (raw \\.\PhysicalDrive access requires admin).
#
#     powershell -ExecutionPolicy Bypass -File .\scripts\sd_write_music.ps1 -DiskNumber 2
#
# Find the disk number first via:
#     Get-Disk | ft Number,FriendlyName,Size,PartitionStyle
# The SD is usually the smallest external disk. Triple-check before
# running -- writing at LBA 524288 on the wrong disk WILL trash data.

param(
    [Parameter(Mandatory = $true)]
    [int]$DiskNumber,

    [string]$Image = "build\wc1_music.img",
    [int]$SeekLBA  = 524288,
    [int]$Sector   = 512
)

if (-not (Test-Path $Image)) {
    Write-Error "image not found: $Image"
    exit 1
}

$ImageSize = (Get-Item $Image).Length
Write-Host "image:        $Image  ($ImageSize bytes)"
Write-Host "target disk:  PhysicalDrive$DiskNumber"
Write-Host "target byte:  $($SeekLBA * $Sector)  (LBA $SeekLBA)"

# Show the target disk so the user can sanity-check before we open it.
$disk = Get-Disk -Number $DiskNumber -ErrorAction Stop
Write-Host ""
Write-Host "target disk identity:"
$disk | Format-List Number, FriendlyName, Manufacturer, Model, SerialNumber, Size, BusType, PartitionStyle | Out-String | Write-Host

$confirm = Read-Host "Write $ImageSize bytes to PhysicalDrive$DiskNumber starting at LBA $SeekLBA? (type 'yes' to proceed)"
if ($confirm -ne 'yes') {
    Write-Host "aborted"
    exit 0
}

$drivePath = "\\.\PhysicalDrive$DiskNumber"
try {
    $sd  = [System.IO.File]::Open($drivePath, 'Open', 'Write', 'ReadWrite')
    $img = [System.IO.File]::OpenRead((Resolve-Path $Image))
    $sd.Seek([int64]$SeekLBA * $Sector, 'Begin') | Out-Null

    $bufSize = 1 * 1024 * 1024
    $buf = New-Object byte[] $bufSize
    $written = 0L
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    while (($n = $img.Read($buf, 0, $bufSize)) -gt 0) {
        $sd.Write($buf, 0, $n)
        $written += $n
        if ($sw.Elapsed.TotalSeconds -ge 1) {
            $pct = [math]::Round(100.0 * $written / $ImageSize, 1)
            Write-Host -NoNewline "`r  $written / $ImageSize  ($pct %)"
            $sw.Restart()
        }
    }
    $sd.Flush()
    Write-Host ""
    Write-Host "OK: wrote $written bytes."
}
finally {
    if ($sd)  { $sd.Close() }
    if ($img) { $img.Close() }
}
