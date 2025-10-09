# Development Tools

This directory contains various development and debugging tools for the Chess AI vs AI project.

## Serial Port Detection Tools

- `quick_detect.py` - Quick serial port device detection
- `detect_board.py` - Comprehensive board detection with multiple baud rates
- `enhanced_detect.py` - Enhanced detection with extensive command testing
- `final_detect.py` - Focused detection for USB serial devices
- `analyze_com6.py` - Specific analysis tool for COM6 data patterns

## PowerShell Scripts

- `get_com_info.ps1` - Get COM9 device information via WMI
- `get_com6_info.ps1` - Get COM6 device information via WMI

## Usage

Most tools can be run directly with Python:

```bash
python tools/quick_detect.py
python tools/analyze_com6.py
```

PowerShell scripts:

```powershell
powershell -ExecutionPolicy Bypass -File tools/get_com6_info.ps1
```