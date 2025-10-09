# ESP32 Framework - Project Summary

## Project Purpose

The **ESP32 Framework** is a generic, reusable development environment extracted from the Chess AI vs AI project. Its purpose is to serve as a robust template for future ESP32 web server applications.

## Overview

This project takes the proven architecture, tooling, and development workflow from the Chess AI vs AI project and transforms it into a general-purpose framework that can be used repeatedly for developing complete ESP32-based web applications.

## Key Features

### Core Architecture
- **ESP32 Web Server**: Fully functional async web server with SD card support
- **Chunked File Streaming**: Memory-efficient file serving (critical for ESP32's limited RAM)
- **SD Card Logging**: Persistent debug logging to SD card with browser integration
- **Admin Authentication**: IP-based admin access control system

### Development Tooling
- **HTML Upload System**: Python scripts for uploading HTML files directly to ESP32 via HTTP (no recompile needed)
- **Log Viewing Tools**: Command-line scripts to view current session logs and crash logs from SD card
- **Version Management**: Automated version tracking tied to git branches
- **Admin IP Management**: CLI tools for managing admin access list

### Development Workflow
- **Rapid Iteration**: Update HTML/JavaScript without recompiling or reuploading ESP32 firmware
- **Live Debugging**: Real-time debug output to browser panel, serial console, and SD card
- **Git Branch Integration**: Version numbers automatically sync with git branch names
- **Command Shortcuts**: Custom CLI commands for common development tasks

## Technology Stack

### Hardware
- ESP32 microcontroller
- SD card module for persistent storage

### Software
- **PlatformIO**: Build system and IDE integration
- **Arduino Framework**: ESP32 development framework
- **AsyncWebServer**: Non-blocking web server library
- **Python**: Development tooling and upload scripts

### Frontend
- HTML5/CSS3/JavaScript
- Responsive design patterns
- WebSocket support (ready for real-time features)

## Use Cases

This framework is ideal for creating:
- IoT control panels and dashboards
- Data logging and monitoring systems
- Smart home interfaces
- Sensor data visualization applications
- Any ESP32 web-based application requiring:
  - File serving from SD card
  - Admin authentication
  - Real-time debugging
  - Rapid HTML/JavaScript development

## Development Philosophy

### Memory Management
- **Always use chunked streaming** for file serving to prevent out-of-memory crashes
- ESP32 has only ~80-100KB free heap - large files must be streamed in chunks

### Logging Standards
- **ESP32/C++**: Use `Serial.printf()` and `Serial.println()` (auto-redirected to SDLogger)
- **JavaScript**: Use `debugLog()` instead of `console.log()` for visibility in Debug Log panel

### Workflow Efficiency
- HTML/JavaScript changes: Upload via `python upload_html.py` (seconds)
- ESP32 code changes: Compile and upload via PlatformIO (minutes)
- No ESP32 reboot needed for HTML updates - just refresh browser

## Project Structure

```
ESP32 Framework/
├── src/                    # ESP32 C++ source code
├── include/                # Header files
├── data/                   # Files to be uploaded to SD card
├── chess-app.html          # Main application HTML (template)
├── upload_html.py          # HTML upload script
├── view_logs.py            # Current session log viewer
├── view_crash_log.py       # Crash log viewer
├── edit_admin_ips.py       # Admin IP management
├── platformio.ini          # PlatformIO configuration
├── CLAUDE.md               # Development instructions and CLI commands
└── PromptHistory.MD        # Development history tracking
```

## Getting Started

1. **Clone this framework** for your new project
2. **Customize chess-app.html** with your application logic
3. **Upload HTML** to ESP32: `python upload_html.py`
4. **Build and upload** ESP32 firmware (one-time)
5. **Iterate rapidly** on HTML/JavaScript with instant uploads

## Branch Strategy

- **main**: Stable releases and major milestones
- **0.0**: Current development branch (first framework version)
- Version numbers automatically sync with branch names

## Future Enhancements

This framework can be extended with:
- WebSocket real-time communication
- REST API endpoints
- Database integration (SD card-based)
- OTA (Over-The-Air) firmware updates
- Multi-page application routing
- User authentication system
- Custom configuration panels

## Credits

Based on the Chess AI vs AI project architecture, refined and generalized for reusable ESP32 web development.

---

**Version**: 0.0 (Framework Development)
**Created**: October 2025
**Purpose**: Reusable ESP32 web server development template
