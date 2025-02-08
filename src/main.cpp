#include <SPI.h>
#include <LoRa.h>
#include "MeshFunctions.h"

// LoRa pins (adjust as needed)
#define ss    5
#define rst   14
#define dio0  2

#define SCK_PIN 18
#define MISO 17
#define MOSI 23
#define SS 5

// Button pin and debounce parameters
const int buttonPin = 26;       // Button connected to pin 26

// Use RELAY_PIN from MeshFunctions.h (defined as 27)

// Global node identifier (adjust per device)
uint8_t myID = 2;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  // Initialize button pin with internal pullup.
  pinMode(buttonPin, INPUT_PULLUP);
  
  // Initialize relay pin using RELAY_PIN from MeshFunctions.h
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Ensure relay is off initially

  Serial.println("LoRa Mesh Node Starting...");

  // Initialize SPI with custom pins
  SPI.begin(SCK_PIN, MISO, MOSI, SS);
  
  // Initialize LoRa with specified pins and frequency
  LoRa.setPins(ss, rst, dio0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);
  }
  Serial.println("LoRa init succeeded.");

  // Initialize recent messages cache is done in MeshFunctions.cpp constructor equivalent (if needed)

}

void loop() {
  // --- Check for incoming LoRa packets ---
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    Serial.print("Received packet: ");
    Serial.println(incoming);
    
    // Expected format: id,src,dest,ttl,payload
    int firstComma = incoming.indexOf(',');
    int secondComma = incoming.indexOf(',', firstComma + 1);
    int thirdComma = incoming.indexOf(',', secondComma + 1);
    int fourthComma = incoming.indexOf(',', thirdComma + 1);
    if (firstComma == -1 || secondComma == -1 || thirdComma == -1 || fourthComma == -1) {
      Serial.println("Malformed message");
      return;
    }
    
    String idStr = incoming.substring(0, firstComma);
    String srcStr = incoming.substring(firstComma + 1, secondComma);
    String destStr = incoming.substring(secondComma + 1, thirdComma);
    String ttlStr = incoming.substring(thirdComma + 1, fourthComma);
    String payload = incoming.substring(fourthComma + 1);
    
    uint32_t msgID = idStr.toInt();
    uint8_t src = (uint8_t)srcStr.toInt();
    uint8_t dest = (uint8_t)destStr.toInt();
    uint8_t ttl = (uint8_t)ttlStr.toInt();
    
    if (isDuplicate(msgID)) {
      Serial.println("Duplicate message, ignoring");
      return;
    }
    addRecentMsgID(msgID);
    
    // If message is for this node or a broadcast (0xFF)
    if (dest == myID || dest == 0xFF) {
      processMessage(payload, msgID, src, dest, ttl);
      if (dest == myID) {
        sendAck(src, msgID);
      }
    }
    
    // Forward message if TTL > 0 and not exclusively for this node
    if (ttl > 0 && dest != myID) {
      ttl--;
      forwardMessage(msgID, src, dest, ttl, payload);
    }
  }

  // --- Update relay state ---
  updateRelay();
  
  
  // --- Button handling ---
  static bool lastButtonState = HIGH;
  bool reading = digitalRead(buttonPin);
  
  // Simple debounce: Only trigger if reading is LOW and stable.
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      Serial.println("Button pressed (debounced)");
      sendMessage(0x01, "Button Pressed from node " + String(myID));
      // Wait a little to prevent multiple triggers
      delay(200);
    }
  }
  lastButtonState = reading;
}
