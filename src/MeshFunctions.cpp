#include "MeshFunctions.h"
#include <LoRa.h>

// --- Duplicate Detection Cache ---
uint32_t recentMsgIDs[MAX_RECENT_MSG];
int recentMsgCount = 0;

// --- Relay Control Variables ---
unsigned long relayStartTime = 0;
bool relayActive = false;
const unsigned long RELAY_DURATION = 1000; // 1 second

// --- Mesh Functions Implementation ---

uint32_t generateUniqueID() {
  // Combine the global node ID (myID) and the lower 24 bits of millis()
  return ((uint32_t)myID << 24) | (millis() & 0x00FFFFFF);
}

void processMessage(String payload, uint32_t msgID, uint8_t src, uint8_t dest, uint8_t ttl) {
  Serial.print("Processing message from node ");
  Serial.print(src);
  Serial.print(": ");
  Serial.println(payload);
  // Application-specific processing goes here.
  openRelay();
}

bool isDuplicate(uint32_t id) {
  for (int i = 0; i < recentMsgCount; i++) {
    if (recentMsgIDs[i] == id)
      return true;
  }
  return false;
}

void addRecentMsgID(uint32_t id) {
  if (recentMsgCount < MAX_RECENT_MSG) {
    recentMsgIDs[recentMsgCount++] = id;
  } else {
    // Shift cache if full (FIFO)
    for (int i = 1; i < MAX_RECENT_MSG; i++) {
      recentMsgIDs[i - 1] = recentMsgIDs[i];
    }
    recentMsgIDs[MAX_RECENT_MSG - 1] = id;
  }
}

void forwardMessage(uint32_t msgID, uint8_t src, uint8_t dest, uint8_t ttl, String payload) {
  String msg = String(msgID) + "," + String(src) + "," + String(dest) + "," + String(ttl) + "," + payload;
  Serial.print("Forwarding message: ");
  Serial.println(msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
}

void sendMessage(uint8_t dest, String payload) {
  uint32_t msgID = generateUniqueID();
  uint8_t ttl = INITIAL_TTL;
  String msg = String(msgID) + "," + String(myID) + "," + String(dest) + "," + String(ttl) + "," + payload;
  Serial.print("Sending message: ");
  Serial.println(msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  addRecentMsgID(msgID);
}

void sendAck(uint8_t dest, uint32_t msgID) {
  String ackPayload = "ACK:" + String(msgID);
  uint8_t ttl = 1; // ACKs use minimal TTL
  String ackMsg = String(msgID) + "," + String(myID) + "," + String(dest) + "," + String(ttl) + "," + ackPayload;
  Serial.print("Sending ACK: ");
  Serial.println(ackMsg);
  LoRa.beginPacket();
  LoRa.print(ackMsg);
  LoRa.endPacket();
}

// --- Relay Control Functions ---
void openRelay() {
  relayStartTime = millis();
  relayActive = true;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Relay On");
}

void closeRelay() {
  relayActive = false;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Relay Off");
}

void updateRelay() {
  if (relayActive && (millis() - relayStartTime >= RELAY_DURATION)) {
    closeRelay();
  }
}

// --- New Helper Functions: Handling Incoming and Transmitted Data ---

// This function checks for incoming LoRa packets, parses them,
// and processes the message by calling processMessage(), sending ACKs,
// and forwarding if necessary.
void handleIncomingPacket() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    Serial.print("Received packet: ");
    Serial.println(incoming);
    
    // Expected message format: msgID,src,dest,ttl,payload
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
    
    // If the message is addressed to this node or is a broadcast (0xFF)
    if (dest == myID || dest == 0xFF) {
      processMessage(payload, msgID, src, dest, ttl);
      // Send an ACK if the message is directly for this node
      if (dest == myID) {
        sendAck(src, msgID);
      }
    }
    
    // If TTL is still positive and the message isn’t exclusively for this node, forward it.
    if (ttl > 0 && dest != myID) {
      ttl--;
      forwardMessage(msgID, src, dest, ttl, payload);
    }
  }
}

// This function handles button press debouncing and sends a message when pressed.
void handleButtonPress(int buttonPin , uint8_t destID) {
  static bool lastButtonState = HIGH;
  bool reading = digitalRead(buttonPin);
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      Serial.print("Button pressed, sending message to node ");
      Serial.println(destID);
      // Example: send a message with destination 0x01
      sendMessage(destID, "Button Pressed from node " + String(myID));
      delay(200); // Prevent multiple triggers
    }
  }
  lastButtonState = reading;
}

