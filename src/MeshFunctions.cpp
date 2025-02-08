#include "MeshFunctions.h"
#include <LoRa.h>

// --- Global Variables for Duplicate Detection ---
uint32_t recentMsgIDs[MAX_RECENT_MSG];
int recentMsgCount = 0;

// --- Global Variables for Relay Control ---
unsigned long relayStartTime = 0;
bool relayActive = false;
const unsigned long RELAY_DURATION = 1000; // 1 second

// --- Mesh Functions Implementation ---

uint32_t generateUniqueID() {
  // Combine node ID (myID) and lower 24 bits of millis()
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
  // Note: LoRa transmission functions will be called from main.cpp where LoRa is initialized.
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
  uint8_t ttl = 1; // ACKs need minimal forwarding
  String ackMsg = String(msgID) + "," + String(myID) + "," + String(dest) + "," + String(ttl) + "," + ackPayload;
  Serial.print("Sending ACK: ");
  Serial.println(ackMsg);
  LoRa.beginPacket();
  LoRa.print(ackMsg);
  LoRa.endPacket();
}

// --- New Relay Control Functions ---

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
