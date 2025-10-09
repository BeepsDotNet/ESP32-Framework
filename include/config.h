#ifndef CONFIG_H
#define CONFIG_H

// Development Configuration
// Set to true during development to enable HTML file upload
// Set to false for production to skip upload and use existing SD card file
#define DEVELOPMENT_MODE true

// SD Card Configuration
#define SD_CS_PIN 5
#define SD_MOSI_PIN 23
#define SD_MISO_PIN 19
#define SD_SCK_PIN 18

// HTML File Configuration
#define HTML_FILE_PATH "/chess-app.html"
#define HTML_UPLOAD_TIMEOUT 30000  // 30 seconds

// Network Configuration
#define WIFI_TIMEOUT 10000  // 10 seconds
#define AP_MODE_TIMEOUT 60000  // 1 minute before switching to AP mode

#endif