# PowerShell script to copy chess-app.html to SD card
Write-Host "🔍 Looking for SD card..." -ForegroundColor Yellow

# Get all removable drives (likely SD cards)
$removableDrives = Get-WmiObject -Class Win32_LogicalDisk | Where-Object { $_.DriveType -eq 2 }

if ($removableDrives.Count -eq 0) {
    Write-Host "❌ No removable drives found. Please insert SD card." -ForegroundColor Red
    exit 1
}

$sourceFile = "chess-app.html"
if (-not (Test-Path $sourceFile)) {
    Write-Host "❌ Source file '$sourceFile' not found!" -ForegroundColor Red
    exit 1
}

foreach ($drive in $removableDrives) {
    $driveLetter = $drive.DeviceID
    $freeSpace = [math]::Round($drive.FreeSpace / 1GB, 2)
    $totalSize = [math]::Round($drive.Size / 1GB, 2)

    Write-Host "📱 Found removable drive: $driveLetter (${freeSpace}GB free / ${totalSize}GB total)" -ForegroundColor Cyan

    try {
        $destination = "$driveLetter\chess-app.html"
        Copy-Item $sourceFile $destination -Force
        Write-Host "✅ Successfully copied $sourceFile to $destination" -ForegroundColor Green

        # Verify the copy
        if (Test-Path $destination) {
            $fileSize = (Get-Item $destination).Length
            Write-Host "📁 File size: $fileSize bytes" -ForegroundColor Green
            exit 0
        }
    }
    catch {
        Write-Host "❌ Failed to copy to $driveLetter - $($_.Exception.Message)" -ForegroundColor Red
    }
}

Write-Host "❌ Could not copy to any removable drive. Please manually copy chess-app.html" -ForegroundColor Red
Read-Host "Press Enter to continue"