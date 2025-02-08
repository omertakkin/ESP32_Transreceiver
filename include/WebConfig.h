#ifndef WEBCONFIG_H
#define WEBCONFIG_H

#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "MeshFunctions.h"

class WebConfig {
public:
    WebConfig(uint8_t relayPin = RELAY_PIN);  // Use default parameter here
    void begin();
    void handleClient();
    void updateRelayState(bool state);

private:
    WebServer server;
    uint8_t relayPin;
    Preferences preferences;
    
    void handleRoot();
    void handleConfigure();
    void handleNotFound();
    void loadSettings();
    void saveSettings();

    String apSSID;
    String apPassword;
    String staSSID;
    String staPassword;
    void handleWiFiConfig();
};

#endif