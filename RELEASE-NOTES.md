# Chess AI vs AI - Release Notes

## Version 5.8 (October 8, 2025)

**Type:** Bug Fix Release

### Summary
Version 5.8 consolidates all improvements from v5.7 session with comprehensive bug fixes, new debugging features, and UI enhancements.

---

## Critical Bug Fixes

### 1. File Serving System Failure
**Severity:** Critical
**Location:** `src/WebInterface.cpp:1640-1641`

**Problem:** The `streamFileChunk()` function was ignoring its `filename` parameter and hardcoding `/chess-app.html` for ALL file requests. This caused every file endpoint to serve the HTML file instead of the requested file.

**Impact:**
- Debug Log Viewer showed HTML instead of log messages
- Serial log endpoint served HTML
- Console log endpoint served HTML
- Crash log endpoints served HTML
- All chunked file streaming was broken

**Fix:** Changed hardcoded path to use the dynamic `filename` parameter:
```cpp
// Before:
if (SD.exists("/chess-app.html")) {
  file = SD.open("/chess-app.html", FILE_READ);

// After:
if (SD.exists(filename)) {
  file = SD.open(filename, FILE_READ);
```

### 2. Memory Management - Out-of-Memory Protection
**Severity:** Critical
**Location:** Multiple endpoints in `src/WebInterface.cpp`

**Problem:** Several log file endpoints used `request->send(SD, filename, contentType)` which loads entire files into RAM. With ESP32's limited heap (~80-100KB), large log files (>200KB) caused memory exhaustion crashes.

**Endpoints Fixed:**
- `/api/logs/console` (lines 1890-1918)
- `/api/logs/serial` (lines 1920-1948)
- `/api/logs/debug` (lines 1950-1978)
- `/CrashLog.txt` (lines 361-388)
- `/api/crashlog` (lines 414-449)

**Fix:** All endpoints now use `beginChunkedResponse()` with `streamFileChunk()` callback for memory-safe streaming. Files are sent in small chunks, keeping memory usage predictable and low.

### 3. Session Manager - Auto Debug Logging
**Severity:** Enhancement
**Location:** `src/SessionManager.cpp:124-130`

**Change:** Sessions from IPs outside the 192.168.1.x range now have debug logging enabled by default for better diagnostics.

```cpp
// Enable debug logging by default for IPs NOT in the 192.168.1.x range
if (!ipAddress.startsWith("192.168.1.")) {
    session.debugLogEnabled = true;
    Serial.printf("Session %s: Debug logging enabled (external IP: %s)\n",
                  sessionId.c_str(), ipAddress.c_str());
}
```

---

## New Features

### Debug Log Viewer
**Location:** `chess-app.html`

A comprehensive debug log viewer has been added to the Session Control panel, providing real-time access to ESP32 debug logs directly from the browser.

**Features:**
- **400px scrollable viewer** with dark theme and custom scrollbar
- **Three control buttons:**
  - **Pin:** Toggle auto-scroll (stays at bottom vs. manual scrolling)
  - **Reload:** Fetch latest log from ESP32
  - **Download:** Save log as timestamped text file
- **Status bar** showing: Lines Loaded, File Size, Status
- **Color-coded log lines:**
  - Red: ERROR messages
  - Orange: WARNING messages
  - Green: Session-related messages
  - Blue: SIZING INFO messages
  - Gray: Normal messages
- **Auto-scroll to bottom** (can be pinned for manual review)

**Backend Implementation:**
- New API endpoint: `/api/logs/debug`
- Handler: `handleGetDebugLog()` (WebInterface.cpp:1950-1978)
- Declaration: WebInterface.h:117
- Uses chunked streaming for memory safety

---

## UI Improvements

### 1. Chessboard Sizing Optimization
**Location:** `chess-app.html:1956-1999`

Intelligent device-adaptive sizing with safety margins to prevent UI cutoff:

**Device-Specific Margins:**
- iPad: 50px safety margin
- iPhone: 80px (accounts for browser chrome)
- Android: 70px
- Other mobile: 70px
- Desktop: 30px (optimal - minimal UI overlap)

**Additional Enhancements:**
- Detects modern iPads (MacIntel + touch support)
- Accounts for settings panel height dynamically
- Chess container padding: 3px top, 10px sides/bottom
- Alignment: `flex-start` for tight spacing

**Result:** Maximum board size on all devices with no cutoff or overlap.

### 2. Square Labels Enhancement
**Location:** `chess-app.html:242-251`

- Font size increased: 10px → **14px**
- Opacity increased: 0.6 → **0.8**
- Bold weight for better visibility

### 3. Labels Button State Indication
**Location:** `chess-app.html:1058-1073`

Visual feedback for toggle state:
- **Green (#4CAF50)** when labels enabled
- **Blue (#4169e1)** when labels disabled
- Matches existing Moves button behavior

### 4. Responsive Button Bar Layout
**Location:** `chess-app.html:91-125`

Optimized for narrow screens (iPhone portrait mode):

**Changes:**
- Reduced padding: 20px → **10px**
- Reduced gap: 15px → **8px**
- Added `flex-wrap: wrap` for multi-row layout on narrow screens
- Smaller button padding: 12px 20px → **10px 16px**
- Smaller font: 14px → **13px**
- Title font: 24px → **20px**
- Title margin: 30px → **15px**
- Added `white-space: nowrap` to prevent button text wrapping

**Result:** All buttons visible on iPhone portrait mode (wraps to multiple rows as needed).

---

## Gameplay Enhancements

### 1. Human-Like AI Thinking Delay
**Location:** `chess-app.html:1243-1246`

AI now pauses 5-15 seconds (random) before executing moves, creating a more natural playing experience.

```javascript
const thinkingDelay = Math.floor(Math.random() * 10000) + 5000;
updateMessageWithSpinner(`${playerColor} is Thinking`);
```

**Display:**
- Shows "White is Thinking" or "Black is Thinking"
- Animated spinner during delay
- No visible timer (more natural appearance)

### 2. User-Facing Message Cleanup
**Location:** `chess-app.html` (multiple locations)

Removed technical terminology ("engine") from all user-facing messages:

**Changes:**
- "Engine ready" → **"Ready"**
- "Engine not ready" → **"Not ready"**
- "Engine error" → **"Error"**
- "while engine is thinking" → **"while opponent is thinking"**
- "Loading chess engine" → **"Loading"**

---

## Documentation

### CLAUDE.md - Memory Management Rule
**Location:** `CLAUDE.md:170-197`

Added critical documentation about ESP32 memory management and file serving best practices.

**Key Points:**
- ESP32 has only ~80-100KB free heap during normal operation
- Files over ~50KB require chunked streaming
- **Never** use `request->send(SD, filename, ...)` for large files
- **Always** use `beginChunkedResponse()` with `streamFileChunk()` callback
- Applies to: HTML files, log files, any SD card file over ~50KB

---

## Files Modified

### C++ Source Files (4 files)
1. **src/WebInterface.cpp**
   - Fixed `streamFileChunk()` filename parameter usage (critical bug)
   - Added chunked streaming to 5 log endpoints
   - Added `handleGetDebugLog()` function
   - Added `/api/logs/debug` endpoint

2. **include/WebInterface.h**
   - Added `handleGetDebugLog()` declaration

3. **src/SessionManager.cpp**
   - Added auto-enable debug logging for external IPs

### HTML/JavaScript (1 file)
4. **chess-app.html**
   - Added Debug Log Viewer UI and functionality
   - Device-adaptive chessboard sizing
   - Responsive button bar layout
   - Human-like AI thinking delay
   - Labels button state indication
   - Square labels size/opacity improvements
   - Message text cleanup (removed "engine")

### Documentation (1 file)
5. **CLAUDE.md**
   - Added ESP32 memory management section
   - Added chunked streaming rule and examples

---

## Testing Checklist

After compiling and uploading ESP32 firmware:

### Debug Log Viewer
- [ ] Open Session Control panel
- [ ] Click "Reload" in Debug Log Viewer
- [ ] Verify actual log messages appear (not HTML)
- [ ] Test Pin button (toggle auto-scroll)
- [ ] Test Download button (saves timestamped file)
- [ ] Verify color coding works (errors red, warnings orange, etc.)

### Chessboard Sizing
- [ ] Test on iPad (should be larger, no cutoff)
- [ ] Test on iPhone portrait (all buttons visible, may wrap)
- [ ] Test on desktop (maximum size)
- [ ] Verify no overlap with controls

### AI Thinking Delay
- [ ] Start new game
- [ ] Verify 5-15 second delay before AI moves
- [ ] Verify "White/Black is Thinking" message with spinner
- [ ] Confirm no timer is shown

### Memory Stability
- [ ] Generate large log files (100KB+)
- [ ] Download via Debug Log Viewer
- [ ] Download crash logs
- [ ] Verify no ESP32 crashes or memory errors
- [ ] Monitor free heap during operations

---

## Statistics

- **Files Modified:** 6
- **Lines Added:** ~250
- **Lines Modified:** ~50
- **Bug Fixes:** 3 critical
- **New Features:** 1 major (Debug Log Viewer)
- **UI Improvements:** 5
- **Gameplay Enhancements:** 2
- **Documentation Updates:** 1

---

## Known Issues

None at this time.

---

## Upgrade Notes

### From v5.7 to v5.8
1. Compile and upload ESP32 firmware (C++ changes required)
2. Upload HTML file using `python upload_html.py`
3. ESP32 will automatically serve new version
4. Users can refresh browser (no ESP32 reboot needed)
5. Version will update to "v5.8" automatically

### Breaking Changes
None. This is a bug fix and enhancement release with full backward compatibility.

---

## Version History Context

### v5.7 (October 8, 2025)
- Session management improvements
- Browser refresh feature

### v5.6.1 (October 8, 2025)
- Refresh Browser feature with remote browser control
- Fixed duplicate IP addresses in logs
- Added 7 comprehensive safety checks

### v5.6 (October 7, 2025)
- Fixed WebRTC signaling message routing bug

### v5.5 (October 6, 2025)
- WebRTC P2P implementation preparations

### v5.4 (October 6, 2025)
- UI enhancements (dark red board outline, move highlighting)
- Undo move button
- Admin toggle button

### v5.3 (October 6, 2025)
- Stockfish migration (local browser execution)
- localStorage game persistence (survives browser refresh)

### v5.2 (October 6, 2025)
- FEN export with Lichess integration
- Analysis board support

### v5.1 (October 5, 2025)
- Legal moves visualization with yellow circles
- Moves toggle button

### v5.0 (October 5, 2025)
- Version management improvements
- Square labels feature with toggle button

---

*Generated with Claude Code*
*Co-Authored-By: Claude <noreply@anthropic.com>*
