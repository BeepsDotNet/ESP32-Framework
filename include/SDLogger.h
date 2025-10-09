#ifndef SD_LOGGER_H
#define SD_LOGGER_H

#include <Arduino.h>
#include <SD.h>
#include <ESPAsyncWebServer.h>

// Global SD logger pointer (defined in main.cpp)
extern class SDLogger* sdLogger;

// Global serial log event source (defined in WebInterface.cpp)
extern AsyncEventSource* g_serialLogEventSource;

class SDLogger : public Print {
private:
    const char* logFilePath = "/DebugMessages.log";
    HardwareSerial& serialPort;
    String lineBuffer;  // Buffer for building complete lines
    bool sdWriteEnabled = true;  // Control SD card writing

public:
    SDLogger(HardwareSerial& port) : serialPort(port), lineBuffer("") {}

    void begin(unsigned long baud) {
        serialPort.begin(baud);
    }

    // Override write to send to Serial, SD card, AND browser via SSE
    size_t write(uint8_t c) override {
        // Write to hardware serial
        serialPort.write(c);

        // Conditionally write to SD card based on sdWriteEnabled flag
        if (sdWriteEnabled) {
            File logFile = SD.open(logFilePath, FILE_APPEND);
            if (logFile) {
                logFile.write(c);
                logFile.flush();  // Force immediate write to SD card
                logFile.close();
            }
        }

        // Build line buffer for SSE
        if (c == '\n') {
            // Complete line - send to browser
            if (lineBuffer.length() > 0 && g_serialLogEventSource) {
                g_serialLogEventSource->send(lineBuffer.c_str(), "serial-log", millis());
            }
            lineBuffer = "";
        } else if (c != '\r') {
            lineBuffer += (char)c;
            // Prevent buffer overflow
            if (lineBuffer.length() > 500) {
                if (g_serialLogEventSource) {
                    g_serialLogEventSource->send(lineBuffer.c_str(), "serial-log", millis());
                }
                lineBuffer = "";
            }
        }

        return 1;
    }

    // Override write for buffer
    size_t write(const uint8_t* buffer, size_t size) override {
        // Write to hardware serial
        serialPort.write(buffer, size);

        // Conditionally write to SD card based on sdWriteEnabled flag
        if (sdWriteEnabled) {
            File logFile = SD.open(logFilePath, FILE_APPEND);
            if (logFile) {
                logFile.write(buffer, size);
                logFile.flush();  // Force immediate write to SD card
                logFile.close();
            }
        }

        // Process buffer for SSE (character by character to handle line breaks)
        for (size_t i = 0; i < size; i++) {
            uint8_t c = buffer[i];
            if (c == '\n') {
                if (lineBuffer.length() > 0 && g_serialLogEventSource) {
                    g_serialLogEventSource->send(lineBuffer.c_str(), "serial-log", millis());
                }
                lineBuffer = "";
            } else if (c != '\r') {
                lineBuffer += (char)c;
                if (lineBuffer.length() > 500) {
                    if (g_serialLogEventSource) {
                        g_serialLogEventSource->send(lineBuffer.c_str(), "serial-log", millis());
                    }
                    lineBuffer = "";
                }
            }
        }

        return size;
    }

    // Pass through other common methods
    int available() { return serialPort.available(); }
    int read() { return serialPort.read(); }
    int peek() { return serialPort.peek(); }
    void flush() { serialPort.flush(); }

    // Clear the log file
    void clearLog() {
        if (SD.exists(logFilePath)) {
            SD.remove(logFilePath);
        }
    }

    // Session Control Functions
    void setSDWriteEnabled(bool enabled) {
        sdWriteEnabled = enabled;
        serialPort.printf("SD card logging %s (messages still broadcast)\n", enabled ? "ENABLED" : "DISABLED");
    }

    bool getSDWriteEnabled() {
        return sdWriteEnabled;
    }

    void clearAllLogs() {
        serialPort.println("Clearing all log files...");

        // Delete main log files
        if (SD.exists("/DebugMessages.log")) {
            SD.remove("/DebugMessages.log");
            serialPort.println("Deleted: DebugMessages.log");
        }

        if (SD.exists("/CrashLog.txt")) {
            SD.remove("/CrashLog.txt");
            serialPort.println("Deleted: CrashLog.txt");
        }

        // Delete all .log and .txt files in /logs/ directory if it exists
        if (SD.exists("/logs/")) {
            File logsDir = SD.open("/logs/");
            if (logsDir && logsDir.isDirectory()) {
                File entry = logsDir.openNextFile();
                while (entry) {
                    String filename = String(entry.name());
                    if (filename.endsWith(".log") || filename.endsWith(".txt")) {
                        String path = String(entry.path());
                        entry.close();
                        SD.remove(path);
                        serialPort.printf("Deleted: %s\n", path.c_str());
                        entry = logsDir.openNextFile();
                    } else {
                        entry.close();
                        entry = logsDir.openNextFile();
                    }
                }
                logsDir.close();
            }
        }

        serialPort.println("All logs cleared - new session started");
    }
};

// Logging macro - use sdLogger if available, otherwise use Serial
#define LOG_PRINT(...) do { if (sdLogger) { sdLogger->print(__VA_ARGS__); } else { Serial.print(__VA_ARGS__); } } while(0)
#define LOG_PRINTLN(...) do { if (sdLogger) { sdLogger->println(__VA_ARGS__); } else { Serial.println(__VA_ARGS__); } } while(0)
#define LOG_PRINTF(...) do { if (sdLogger) { sdLogger->printf(__VA_ARGS__); } else { Serial.printf(__VA_ARGS__); } } while(0)

// Automatically redirect ALL Serial calls to SDLogger throughout the application
// This ensures all logs appear in: hardware serial, SD card, AND browser debug panel
// Note: Serial is still available before sdLogger is initialized (in early boot)
#undef Serial
#define Serial (sdLogger ? (*sdLogger) : ::Serial)

#endif // SD_LOGGER_H
