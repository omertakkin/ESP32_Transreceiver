#include "WebConfig.h"

// Constructor with default parameter
WebConfig::WebConfig(uint8_t relayPin) : server(80), relayPin(relayPin) {}

void WebConfig::begin() {
    preferences.begin("mesh-config", false); // Initialize NVS storage

    // Load configuration
    staSSID = preferences.getString("staSSID", "");
    staPassword = preferences.getString("staPassword", "");
    relayPin = preferences.getUInt("relayPin", relayPin); // Use constructor value as default

    // Try to connect to STA (your WiFi) first
    if (staSSID != "") {
        Serial.println("Attempting WiFi connection...");
        WiFi.mode(WIFI_STA);
        WiFi.begin(staSSID.c_str(), staPassword.c_str());

        // Wait for connection with timeout
        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nSTA Connected!");
            Serial.print("IP Address: ");
            Serial.println(WiFi.localIP());
            pinMode(relayPin, OUTPUT); // Apply loaded pin mode
            digitalWrite(relayPin, LOW);
            server.begin();
            return; // Skip AP setup if STA connected
        }
    }

    // Fallback to AP Mode with custom IP
    Serial.println("Starting AP Mode");
    IPAddress apIP(192, 168, 4, 1);       // Custom AP IP
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(apIP, gateway, subnet);
    WiFi.softAP("LoRa-Mesh-Node", "configure123"); // AP credentials

    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

    // Configure relay pin from saved settings
    pinMode(relayPin, OUTPUT);
    digitalWrite(relayPin, LOW);

    // Set up server routes
    server.on("/", std::bind(&WebConfig::handleRoot, this));
    server.on("/configure", HTTP_POST, std::bind(&WebConfig::handleConfigure, this));
    server.onNotFound(std::bind(&WebConfig::handleNotFound, this));
    
    server.begin();
}

void WebConfig::handleClient() {
    server.handleClient();
}

void WebConfig::handleRoot() {
    String html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <title>LoRa Mesh Config</title>
        <style>
            body { font-family: Arial; margin: 20px; }
            .section { margin-bottom: 20px; padding: 15px; border: 1px solid #ddd; }
            input, select { margin-bottom: 10px; }
        </style>
    </head>
    <body>
        <h1>LoRa Mesh Configuration</h1>
        
        <div class="section">
            <h2>WiFi Settings</h2>
            <form action="/configure" method="POST">
                SSID: <input type="text" name="ssid" value=")" + staSSID + R"("><br>
                Password: <input type="password" name="password"><br>
                <input type="submit" name="wifi" value="Save WiFi">
            </form>
        </div>

        <div class="section">
            <h2>GPIO Settings</h2>
            <form action="/configure" method="POST">
                Relay Pin: <input type="number" name="pin" min="0" max="39" value=")" + String(relayPin) + R"(" required><br>
                Mode: 
                <select name="mode">
                    <option value="output">Output</option>
                    <option value="input">Input</option>
                </select><br>
                <input type="submit" name="gpio" value="Save GPIO">
            </form>
        </div>
    </body>
    </html>
    )";
    server.send(200, "text/html", html);
}

void WebConfig::handleConfigure() {
    if (server.method() != HTTP_POST) {
        server.send(405, "text/plain", "Method Not Allowed");
        return;
    }

    // Handle WiFi Configuration
    if (server.hasArg("ssid")) {
        staSSID = server.arg("ssid");
        staPassword = server.arg("password");
        preferences.putString("staSSID", staSSID);
        preferences.putString("staPassword", staPassword);
        server.send(200, "text/plain", "WiFi settings saved. Restarting...");
        delay(1000);
        ESP.restart();
    }

    // Handle GPIO Configuration
    if (server.hasArg("pin")) {
        relayPin = server.arg("pin").toInt();
        String mode = server.arg("mode");
        
        preferences.putUInt("relayPin", relayPin);
        pinMode(relayPin, (mode == "output") ? OUTPUT : INPUT);
        
        server.send(200, "text/plain", "GPIO " + String(relayPin) + " configured as " + mode);
    }
}

void WebConfig::loadSettings() {
    // Use the member variable's current value as default
    relayPin = preferences.getUInt("relayPin", relayPin);
    pinMode(relayPin, OUTPUT);
}

void WebConfig::updateRelayState(bool state) {
    digitalWrite(relayPin, state);
}

void WebConfig::handleNotFound() {
    server.send(404, "text/plain", "Not found");
}