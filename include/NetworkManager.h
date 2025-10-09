#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>

enum NetworkStatus {
  NET_DISCONNECTED,
  NET_CONNECTING,
  NET_CONNECTED,
  NET_ERROR
};

class NetworkManager {
private:
  NetworkStatus status;
  String ssid;
  String password;
  unsigned long lastConnectionCheck;
  unsigned long reconnectInterval;
  int reconnectAttempts;
  int maxReconnectAttempts;

public:
  NetworkManager();

  bool connect(const char* ssid, const char* password);
  void disconnect();
  void update();

  NetworkStatus getStatus() const { return status; }
  bool isConnected() const { return status == NET_CONNECTED; }
  String getIPAddress() const;
  int getSignalStrength() const;

  void setReconnectInterval(unsigned long interval) { reconnectInterval = interval; }
  void setMaxReconnectAttempts(int attempts) { maxReconnectAttempts = attempts; }
};

#endif