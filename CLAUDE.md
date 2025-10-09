## File Management Commands
### Upload HTML to ESP32 (Development Mode)
```bash
# Primary method: HTTP upload to ESP32
python upload_html.py


## Development Workflow
2. You run: `python upload_html.py`
3. ESP32 saves HTML file to SD card
4. ESP32 automatically serves from SD card using chunked streaming
5. Test the updated application

## Production Workflow
2. Build and upload ESP32 code
3. ESP32 serves existing HTML file from SD card

- When the cli text is "update version" send following string to agent "Review the current conversation and append ALL user prompts from this session that are not yet in PromptHistory.MD (check the last entries to avoid duplicates)". Update version text with current git branch version"

- When the cli text is "update history" send following string to agent: "Review the current conversation and append ALL user prompts from this session that are not yet in PromptHistory.MD (check the last entries to avoid duplicates)"

- When the cli text is "upload html" send following string to agent: "Update version text with current git branch version, then Bash(python upload_html.py)"

## Version Update Protocol
**Version is defined in ONE location only:**
- Line ~10 in chess-app.html: `const APP_VERSION = "v5.0";`

**To update version:**
1. Update ONLY the APP_VERSION constant at line ~10
2. Upload HTML to ESP32
3. User refreshes browser (no ESP32 reboot needed)

**How it works:**
- The APP_VERSION constant is used dynamically throughout the app
- Browser tab title and page header are updated via JavaScript for admin users
- No hard-coded version strings exist elsewhere in the HTML

## Debugging Commands

### View Current Session Log
```bash
python view_logs.py
```
Shows the current ESP32 session debug log (DebugMessages.log)

### View Crash Log
```bash
python view_crash_log.py
```
Shows archived logs from previous ESP32 sessions (CrashLog.txt). Use this after a crash/reboot to see what happened before the system went down.

- When the cli text is "view log" send following string to agent: "Bash(python view_logs.py --ip 192.168.1.208)"

- When the cli text is "view crash log" send following string to agent: "Bash(python view_crash_log.py)"

- When the cli text is "new branch" send following to the agent 
Review the current conversation and append ALL user prompts that are not yet in PromptHistory.MD (check the last entries to avoid duplicates).
Git push all changed files in the current branch and do a git commit.
git publish the current branch and create a new branch.
update version text with the new branch version

- When the cli text is "create summary" send following string to agent: "Create a comprehensive summary of all changes made in this session and over-write it to summary.md in the project root directory"

- When the cli text is "show commands" display the following command reference:

## Available CLI Commands

**`update version`**
    Update PromptHistory.MD with session prompts and update version text
    to match current git branch
**`update history`**
    Review conversation and append all user prompts to PromptHistory.MD
    (avoids duplicates)
**`upload html`**
    Update version text with current branch version, then upload HTML
    to ESP32
**`view log`**
    Display current ESP32 session debug log (DebugMessages.log)
    Uses chunked streaming to handle large files
    Options:
      --lines N     Show only last N lines
      --save        Save complete log to timestamped file
      --open        Open saved log in Notepad (requires --save)
    Examples:
      view log                    # Show last 50 lines (default)
      view log --lines 100        # Show last 100 lines
      view log --save --open      # Save and open in Notepad
**`view crash log`**
    Display archived crash logs from previous ESP32 sessions
**`new branch`**
    Update PromptHistory.MD, commit and push current branch, publish
    branch, create new branch, update version
**`create summary`**
    Generate comprehensive summary of session changes and write to
    summary.md
**`auth`** or **`auth /l`**
    List all admin IP addresses
**`auth <ip> /a`**
    Add IP address to admin list
    Example: auth 192.168.1.38 /a
**`auth <ip> /d`**
    Remove IP address from admin list
    Example: auth 192.168.1.100 /d
**`auth /h`**
    Show auth command help/usage information
**`show commands`**
    Display this command reference

- When the cli text starts with "auth" send following to agent based on the parameters:
  - "auth /l" or "auth" (no params) → "Bash(python edit_admin_ips.py list)"
  - "auth <ip> /a" → "Bash(python edit_admin_ips.py add <ip>)"
  - "auth <ip> /d" → "Bash(python edit_admin_ips.py remove <ip>)"
  - "auth /h" → "Bash(python edit_admin_ips.py)"
  Examples:
  - "auth" → lists admin IPs
  - "auth 192.168.1.38 /a" → adds IP to admin list
  - "auth 192.168.1.100 /d" → removes IP from admin list
  - "auth /h" → shows help/usage information

- The following files will always stay as local files, never uploaded to GIT:
WIFI.MD
API_Keys.MD
Lichess API Key.png
GEMINI-API-KEY.MD
admin-auth.md

- The user will always manually do compiles and uploads unless directed not to.

## Code Style Guidelines

### Logging/Debug Output

#### ESP32/C++ Code
IMPORTANT: This codebase does NOT use `DebugMessage()` function.

**Always use these patterns for logging:**
```cpp
Serial.printf("Message with format: %s\n", variable.c_str());  // Formatted output
Serial.println("Simple message");                               // Simple output
```

**Never use:**
```cpp
DebugMessage("...");  // Does not exist in this codebase
```

The `Serial` object is automatically redirected to SDLogger (which logs to: hardware serial, SD card, and browser debug panel) via macro in `SDLogger.h`.

#### JavaScript/HTML Code
IMPORTANT: For debugging chess-app.html JavaScript, always use `debugLog()` instead of `console.log()`.

**Always use:**
```javascript
debugLog('Your debug message here');
```

**Never use:**
```javascript
console.log('...');  // Only shows in browser console, not in Debug Log panel
```

**Why:** The `debugLog()` function automatically:
- Displays messages in the Debug Log panel (visible to user)
- Logs to browser console (for F12 debugging)
- Sends logs to ESP32 SD card (for persistent logging)
- Adds timestamps and source prefixes

This ensures all debug output appears in both the app's Debug Log panel AND the browser console, making it easier to debug without switching between tools.

### ESP32 Memory Management - File Serving

**CRITICAL RULE: ALWAYS use chunked streaming for file serving - ESP32 has limited RAM**

The ESP32 has only ~80-100KB of free heap during normal operation. Large files (especially log files that can exceed 200KB) will cause out-of-memory crashes if loaded entirely into RAM.

**Always use:**
```cpp
AsyncWebServerResponse *response = request->beginChunkedResponse("text/plain",
  [this, filename](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    return streamFileChunk(buffer, maxLen, index, filename);
  });

response->addHeader("Cache-Control", "no-cache");
request->send(response);
```

**Never use:**
```cpp
request->send(SD, filename, "text/plain");  // Loads file into memory - will crash on large files!
```

**Why:** The `request->send(SD, ...)` convenience method can cause memory exhaustion with large files. Chunked streaming keeps memory usage predictable and low by reading and sending the file in small chunks.

**This applies to:**
- HTML files
- Log files (DebugMessages.log, CrashLog.txt)
- Any file from SD card over ~50KB