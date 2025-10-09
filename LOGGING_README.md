# Chess AI Logging System

Complete logging system for debugging the Chess AI application on ESP32.

## Log Files (on SD Card)

1. **`/console_messages.log`** - Browser/JavaScript debug messages
   - Player moves
   - Opponent moves
   - SSE events
   - Game state changes
   - Errors

2. **`/serial_monitor_messages.log`** - ESP32 serial output
   - System initialization
   - WiFi connection
   - API calls
   - Stream events
   - Move processing
   - All timestamped debug messages

## Web Endpoints

### View Logs
- `GET http://<ESP32_IP>/api/logs/console` - Get console log
- `GET http://<ESP32_IP>/api/logs/serial` - Get serial monitor log

### Clear Logs
- `POST http://<ESP32_IP>/api/logs/clear` - Clear both log files

## Python Tools

### 1. View Logs (`view_logs.py`)

View and save log files from ESP32.

**Basic usage:**
```bash
# View both logs
python view_logs.py

# View specific log
python view_logs.py console
python view_logs.py serial

# Specify IP address
python view_logs.py -i 192.168.1.208

# Save to files
python view_logs.py -s

# Show only last 50 lines
python view_logs.py -l 50

# Clear logs on ESP32
python view_logs.py -c
```

**Examples:**
```bash
# View serial log from specific IP and save
python view_logs.py serial -i 192.168.1.208 -s

# View last 100 lines of both logs
python view_logs.py both -l 100

# Clear all logs
python view_logs.py -c
```

### 2. Tail Logs (`tail_logs.py`)

Live view of logs as they're written (like `tail -f` on Linux).

**Basic usage:**
```bash
# Tail serial log (default)
python tail_logs.py

# Tail console log
python tail_logs.py console

# Specify IP and update interval
python tail_logs.py -i 192.168.1.208 -t 1.0
```

**Examples:**
```bash
# Watch serial log from custom IP, update every second
python tail_logs.py serial -i 192.168.1.208 -t 1

# Watch browser console log
python tail_logs.py console
```

Press Ctrl+C to stop tailing.

## Browser Debug Panel

Click the green **"Debug Log"** button in the chess app to show/hide the debug panel.

- Shows last 15 log entries with timestamps
- Updates in real-time
- Works on iPad/Safari (no dev tools needed)
- Also sends logs to SD card

## Log Behavior

- **Logs persist across reboots** - append mode with session markers
- **Session markers** added on each boot:
  ```
  ========================================
  === New Session Started ===
  === Timestamp: 12345 ms ===
  ========================================
  ```
- **Automatic logging** - no need to run Serial Monitor in VSCode
- **Dual output** - Serial monitor still works if you want to watch live

## Debugging Workflow

### Finding Intermittent Issues

1. **Start fresh (optional):**
   ```bash
   python view_logs.py -c
   ```

2. **Play multiple games** until issue occurs

3. **View logs:**
   ```bash
   # Quick view - last 100 lines
   python view_logs.py -l 100

   # Save full logs for analysis
   python view_logs.py -s
   ```

4. **Or tail logs live:**
   ```bash
   python tail_logs.py
   ```

5. **Analyze patterns:**
   - Look for last event before lockup
   - Check for missing opponent moves
   - Verify stream events are arriving
   - Look for timeout messages

### Log File Timestamps

All critical events are timestamped in milliseconds:
- `[12345] MOVE START: e2e4 (game: abc123)`
- `[12456] MOVE ACCEPTED: e2e4`
- `[12567] Stream resumed - listening for opponent`
- `[12789] STREAM EVENT: {"type":"gameState"...}`

Compare timestamps to identify delays/hangs.

## Requirements

Python 3.6+ with `requests` library:
```bash
pip install requests
```

## Tips

- Use **tail_logs.py** during active gameplay for real-time monitoring
- Use **view_logs.py** after issue occurs for detailed analysis
- Logs accumulate across sessions - look for patterns over multiple games
- Clear logs periodically to keep file sizes manageable
- Browser debug panel is useful on mobile devices without console access
