#include "NetworkManager.h"

NetworkManager::NetworkManager() {
  status = NET_DISCONNECTED;
  lastConnectionCheck = 0;
  reconnectInterval = 30000; // 30 seconds
  reconnectAttempts = 0;
  maxReconnectAttempts = 5;
}

bool NetworkManager::connect(const char* wifiSSID, const char* wifiPassword) {
  ssid = String(wifiSSID);
  password = String(wifiPassword);

  Serial.printf("Connecting to WiFi network: %s\n", ssid.c_str());

  status = NET_CONNECTING;
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSSID, wifiPassword);

  unsigned long startTime = millis();
  unsigned long timeout = 30000; // 30 second timeout

  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeout) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    status = NET_CONNECTED;
    reconnectAttempts = 0;

    Serial.println();
    Serial.printf("WiFi connected successfully!\n");
    Serial.printf("IP address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Gateway: %s\n", WiFi.gatewayIP().toString().c_str());
    Serial.printf("Subnet: %s\n", WiFi.subnetMask().toString().c_str());
    Serial.printf("DNS: %s\n", WiFi.dnsIP().toString().c_str());
    Serial.printf("Signal strength: %d dBm\n", WiFi.RSSI());

    return true;
  } else {
    status = NET_ERROR;
    Serial.println();
    Serial.printf("WiFi connection failed. Status: %d\n", WiFi.status());

    switch (WiFi.status()) {
      case WL_NO_SSID_AVAIL:
        Serial.println("Error: SSID not available");
        break;
      case WL_CONNECT_FAILED:
        Serial.println("Error: Connection failed");
        break;
      case WL_CONNECTION_LOST:
        Serial.println("Error: Connection lost");
        break;
      // WL_WRONG_PASSWORD not available in newer ESP32 WiFi library
      case WL_DISCONNECTED:
        Serial.println("Error: Disconnected");
        break;
      default:
        Serial.printf("Error: Unknown status %d\n", WiFi.status());
        break;
    }

    return false;
  }
}

void NetworkManager::disconnect() {
  Serial.println("Disconnecting from WiFi...");
  WiFi.disconnect(true);
  status = NET_DISCONNECTED;
  reconnectAttempts = 0;
}

void NetworkManager::update() {
  unsigned long currentTime = millis();

  // Check connection status every 5 seconds
  if (currentTime - lastConnectionCheck >= 5000) {
    lastConnectionCheck = currentTime;

    wl_status_t wifiStatus = WiFi.status();

    switch (status) {
      case NET_CONNECTED:
        if (wifiStatus != WL_CONNECTED) {
          Serial.println("WiFi connection lost");
          status = NET_DISCONNECTED;
          reconnectAttempts = 0;
        }
        break;

      case NET_DISCONNECTED:
      case NET_ERROR:
        if (wifiStatus == WL_CONNECTED) {
          Serial.println("WiFi reconnected automatically");
          status = NET_CONNECTED;
          reconnectAttempts = 0;
        } else if (currentTime - lastConnectionCheck >= reconnectInterval) {
          // Attempt to reconnect
          if (reconnectAttempts < maxReconnectAttempts) {
            reconnectAttempts++;
            Serial.printf("Attempting to reconnect (attempt %d/%d)...\n",
              reconnectAttempts, maxReconnectAttempts);

            if (connect(ssid.c_str(), password.c_str())) {
              Serial.println("Reconnection successful");
            } else {
              Serial.printf("Reconnection failed (attempt %d)\n", reconnectAttempts);

              if (reconnectAttempts >= maxReconnectAttempts) {
                Serial.println("Max reconnection attempts reached");
                status = NET_ERROR;
              }
            }
          }
        }
        break;

      case NET_CONNECTING:
        if (wifiStatus == WL_CONNECTED) {
          status = NET_CONNECTED;
          Serial.println("Connection established during update check");
        } else if (currentTime - lastConnectionCheck > 30000) {
          // Connection timeout
          status = NET_ERROR;
          Serial.println("Connection timeout during update");
        }
        break;
    }
  }
}

String NetworkManager::getIPAddress() const {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  return "Not connected";
}

int NetworkManager::getSignalStrength() const {
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.RSSI();
  }
  return -100; // Very weak signal as default
}