// WiFi Configuration Example
// Copy this file to src/wifi_config.h and update with your credentials

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

// WiFi credentials
const char* WIFI_SSID = "your-wifi-network-name";
const char* WIFI_PASSWORD = "your-wifi-password";

// Optional: Static IP configuration
// Comment out to use DHCP
/*
#define USE_STATIC_IP
#ifdef USE_STATIC_IP
  IPAddress local_IP(192, 168, 1, 184);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  IPAddress primaryDNS(8, 8, 8, 8);
  IPAddress secondaryDNS(8, 8, 4, 4);
#endif
*/

// Web server port (default: 80)
#define WEB_SERVER_PORT 80

// AI API Configuration
// Note: For security, consider storing API keys on SD card instead of in code
#define OPENAI_API_KEY "your-openai-api-key-here"
#define ANTHROPIC_API_KEY "your-anthropic-api-key-here"

// Game configuration
#define AI_MOVE_TIMEOUT_MS 60000  // 60 seconds
#define GAME_SAVE_INTERVAL_MS 5000  // Save game state every 5 seconds
#define MAX_ARCHIVED_GAMES 10  // Keep last 10 games

#endif