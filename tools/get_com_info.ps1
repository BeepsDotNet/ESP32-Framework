Get-WmiObject -Class Win32_SerialPort | Where-Object {$_.DeviceID -eq 'COM9'} | Select-Object Name,Description,Manufacturer,DeviceID

# Also try PnP devices
Get-WmiObject -Class Win32_PnPEntity | Where-Object {$_.Name -like "*COM9*"} | Select-Object Name,Description,Manufacturer