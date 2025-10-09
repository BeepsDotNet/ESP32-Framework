@echo off
echo Copying chess-app.html to SD card...

REM Try common SD card drive letters
for %%d in (D E F G H I J) do (
    if exist %%d:\ (
        echo Found drive %%d: - checking if it's SD card...
        copy "chess-app.html" "%%d:\chess-app.html" >nul 2>&1
        if not errorlevel 1 (
            echo ✅ Successfully copied chess-app.html to %%d:\
            goto :done
        )
    )
)

echo ❌ Could not find SD card. Please manually copy chess-app.html
echo Available drives:
wmic logicaldisk get size,freespace,caption

:done
pause