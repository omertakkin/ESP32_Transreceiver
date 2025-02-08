#include "MeshFunctions.h"
#include <SPI.h>
#include <LoRa.h>

// Global State
uint8_t myID = 1;
unsigned long relayStartTime = 0;
bool relayActive = false;

// Message Tracking
uint32_t recentMsgIDs[MAX_RECENT_MSG];
int recentMsgIndex = 0;
bool recentMsgFull = false;

// ACK Management
PendingMessage pendingMessages[MAX_PENDING_MSGS];
uint8_t pendingCount = 0;

// Packet Serialization
void serializePacket(const MeshPacket &packet, uint8_t* buffer) {
  memcpy(buffer, &packet, sizeof(MeshPacket));
}

void deserializePacket(const uint8_t* buffer, MeshPacket &packet) {
  memcpy(&packet, buffer, sizeof(MeshPacket));
}

// Duplicate Detection
bool isDuplicate(uint32_t id) {
  for(int i=0; i<(recentMsgFull ? MAX_RECENT_MSG : recentMsgIndex); i++) {
    if(recentMsgIDs[i] == id) return true;
  }
  Serial.println("Message Duplication Detected");
  return false;
}

void addRecentMsgID(uint32_t id) {
  recentMsgIDs[recentMsgIndex] = id;
  recentMsgIndex = (recentMsgIndex + 1) % MAX_RECENT_MSG;
  if(!recentMsgFull && recentMsgIndex == 0) recentMsgFull = true;
}

// Message Handling
void processMessage(MeshPacket &packet) {
  Serial.printf("CMD %d from %d (Payload: %.2f)\n", 
               packet.command, packet.src, packet.payload);

  switch(packet.command) {
    case CMD_OPEN_RELAY:
      openRelay();
      break;
    case CMD_CLOSE_RELAY:
      closeRelay();
      break;
  }
}

void forwardMessage(MeshPacket packet) {
  if(packet.ttl > 0) {
    packet.ttl--;
    uint8_t buffer[sizeof(MeshPacket)];
    serializePacket(packet, buffer);

    if(LoRa.beginPacket() && (LoRa.getTimeout() != LORA_TIMEOUT_MS)) {
      LoRa.setTimeout(LORA_TIMEOUT_MS);
    }
    
    if(LoRa.beginPacket()) {
      LoRa.write(buffer, sizeof(buffer));
      LoRa.endPacket();
    }
  }
}

// Message Transmission
void sendMessage(uint8_t dest, uint8_t command, float_t payload) {
  MeshPacket packet;
  packet.msgID = (myID << 24) | (micros() & 0x00FFFFFF);
  packet.src = myID;
  packet.dest = dest;
  packet.ttl = INITIAL_TTL;
  packet.command = command;
  packet.payload = payload;

  uint8_t buffer[sizeof(MeshPacket)];
  serializePacket(packet, buffer);

  if(LoRa.beginPacket()) {
    LoRa.write(buffer, sizeof(buffer));
    LoRa.endPacket();
    addRecentMsgID(packet.msgID);
  }

  // Queue for ACK tracking if needed
  if(dest != 0xFF && command != CMD_ACK && pendingCount < MAX_PENDING_MSGS) {
    pendingMessages[pendingCount++] = {
      packet.msgID,
      dest,
      command,
      payload,
      millis(),
      0
    };
  }
}

void sendAck(uint8_t dest, uint32_t msgID) {
  MeshPacket packet;
  packet.msgID = msgID;
  packet.src = myID;
  packet.dest = dest;
  packet.ttl = 1;
  packet.command = CMD_ACK;
  packet.payload = 0.0f;

  uint8_t buffer[sizeof(MeshPacket)];
  serializePacket(packet, buffer);

  if(LoRa.beginPacket()) {
    LoRa.write(buffer, sizeof(buffer));
    LoRa.endPacket();
  }
}

// ACK Management
void checkPendingAcks() {
  unsigned long now = millis();
  
  for(int i=0; i<pendingCount; ) {
    PendingMessage &pm = pendingMessages[i];
    
    if(now - pm.sendTime > ACK_TIMEOUT) {
      if(pm.retries < MAX_RETRIES) {
        Serial.printf("Retrying message %08X (Attempt %d)\n", pm.msgID, pm.retries+1);
        sendMessage(pm.dest, pm.command, pm.payload);
        pm.sendTime = now;
        pm.retries++;
        i++;
      } else {
        Serial.printf("Failed message %08X after %d retries\n", pm.msgID, MAX_RETRIES);
        memmove(&pendingMessages[i], &pendingMessages[i+1], 
               (pendingCount-i-1)*sizeof(PendingMessage));
        pendingCount--;
      }
    } else {
      i++;
    }
  }
}

// Network Handling
void handleIncomingPacket() {
  int packetSize = LoRa.parsePacket();
  if(packetSize == sizeof(MeshPacket)) {
    uint8_t buffer[sizeof(MeshPacket)];
    LoRa.readBytes(buffer, sizeof(buffer));
    
    MeshPacket packet;
    deserializePacket(buffer, packet);

    // Process ACK messages first, bypassing duplicate detection:
    if(packet.command == CMD_ACK) {
      Serial.println("ACK message received");
      for(int i = 0; i < pendingCount; i++) {
        if(pendingMessages[i].msgID == packet.msgID) {
          memmove(&pendingMessages[i], &pendingMessages[i+1],
                  (pendingCount - i - 1) * sizeof(PendingMessage));
          pendingCount--;
          break;
        }
      }
      Serial.print("ACK Handled, New pendingCounter: ");
      Serial.println(pendingCount);
      return;
    }

    // For non-ACK messages, do duplicate detection
    if(isDuplicate(packet.msgID)) return;
    addRecentMsgID(packet.msgID);

    Serial.print("Cmd: ");
    Serial.println(packet.command);

    // Process valid messages for this node
    if(packet.dest == myID || packet.dest == 0xFF) {
      Serial.println("Valid message received");
      processMessage(packet);
      // If the message is intended for this node (and is not an ACK), send an ACK back:
      if(packet.dest == myID && packet.command != CMD_ACK) {
        sendAck(packet.src, packet.msgID);
        Serial.println("ACK message sent");
      }
    }

    // Forward the message if needed
    if(packet.ttl > 0 && packet.dest != myID) {
      forwardMessage(packet);
      Serial.println("Message Forwarded");
    }
  }
}

// Relay Control
void openRelay() {
  relayStartTime = millis();
  relayActive = true;
  digitalWrite(RELAY_PIN, HIGH);
}

void closeRelay() {
  relayActive = false;
  digitalWrite(RELAY_PIN, LOW);
}

void updateRelay() {
  if(relayActive && (millis() - relayStartTime >= RELAY_DURATION)) {
    closeRelay();
  }
}

// Input Handling
void handleButtonPress(int buttonPin, uint8_t destID, uint8_t cmdID) {
  static unsigned long lastPress = 0;
  const unsigned long debounce = 50;
  
  if(digitalRead(buttonPin) == LOW && (millis() - lastPress) > debounce) {
    lastPress = millis();
    sendMessage(destID, cmdID, 0);
    Serial.println("Message Sent");
  }
}