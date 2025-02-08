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

// Generate a unique message ID combining the node's ID and the current millis() value.
uint32_t generateUniqueID() {
    // Combine the global node ID (myID) and the lower 24 bits of millis()
  return ((uint32_t)myID << 24) | (millis() & 0x00FFFFFF);
}

// Process a received message contained in a MeshPacket.
void processMessage(MeshPacket &packet) {
  Serial.print("Processing message from node ");
  Serial.print(packet.src);
  Serial.print(": ");
  Serial.println(packet.payload);
  // Application-specific processing goes here.
  openRelay();
}

// Check if a message ID has been seen before.
bool isDuplicate(uint32_t id) {
  for (int i = 0; i < recentMsgCount; i++) {
    if (recentMsgIDs[i] == id)
      return true;
  }
  return false;
}

// Add a message ID to the recent cache.
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

// Forward a message after decrementing its TTL.
// The packet is passed by value so that the original packet remains unchanged.
void forwardMessage(MeshPacket packet) {
  if (packet.ttl > 0) {
    packet.ttl--; // Decrement TTL for the forwarded message
    String msg = serializePacket(packet);
    Serial.print("Forwarding message: ");
    Serial.println(msg);
    LoRa.beginPacket();
    LoRa.print(msg);
    LoRa.endPacket();
  }
}

// Send a message by creating a MeshPacket, serializing it, and transmitting.
void sendMessage(uint8_t dest, String payload) {
  MeshPacket packet;
  packet.msgID = generateUniqueID();
  packet.src = myID;
  packet.dest = dest;
  packet.ttl = INITIAL_TTL;
  packet.payload = payload;

  String msg = serializePacket(packet);
  Serial.print("Sending message: ");
  Serial.println(msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  addRecentMsgID(packet.msgID);
}

// Send an ACK packet to a node acknowledging receipt of a message.
void sendAck(uint8_t dest, uint32_t msgID) {
  MeshPacket packet;
  packet.msgID = msgID; // Use the original message ID
  packet.src = myID;
  packet.dest = dest;
  packet.ttl = 1; // Minimal TTL for ACK
  packet.payload = "ACK:" + String(msgID);

  String msg = serializePacket(packet);
  Serial.print("Sending ACK: ");
  Serial.println(msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
}

// --- Packet Serialization Functions ---

// Convert a MeshPacket into a CSV string: msgID,src,dest,ttl,payload
String serializePacket(const MeshPacket &packet) {
  return String(packet.msgID) + "," + 
         String(packet.src) + "," + 
         String(packet.dest) + "," + 
         String(packet.ttl) + "," + 
         packet.payload;
}

// Parse a CSV string into a MeshPacket.
// Expected format: msgID,src,dest,ttl,payload
MeshPacket deserializePacket(const String &data) {
  MeshPacket packet;
  int idx1 = data.indexOf(',');
  int idx2 = data.indexOf(',', idx1 + 1);
  int idx3 = data.indexOf(',', idx2 + 1);
  int idx4 = data.indexOf(',', idx3 + 1);
  
  if (idx1 == -1 || idx2 == -1 || idx3 == -1 || idx4 == -1) {
    Serial.println("Malformed packet");
    return packet; // Returns a packet with default values.
  }
  
  packet.msgID   = data.substring(0, idx1).toInt();
  packet.src     = (uint8_t) data.substring(idx1 + 1, idx2).toInt();
  packet.dest    = (uint8_t) data.substring(idx2 + 1, idx3).toInt();
  packet.ttl     = (uint8_t) data.substring(idx3 + 1, idx4).toInt();
  packet.payload = data.substring(idx4 + 1);
  
  return packet;
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

// --- Incoming Packet and Button Handling ---

// Check for incoming LoRa packets, deserialize them into MeshPacket,
// check for duplicates, process the packet, send ACK if needed, and forward it.
void handleIncomingPacket() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) {
      incoming += (char)LoRa.read();
    }
    Serial.print("Received packet: ");
    Serial.println(incoming);
    
    MeshPacket packet = deserializePacket(incoming);
    
    if (isDuplicate(packet.msgID)) {
      Serial.println("Duplicate message, ignoring");
      return;
    }
    addRecentMsgID(packet.msgID);
    
    // Process if the message is addressed to this node or is a broadcast (0xFF)
    if (packet.dest == myID || packet.dest == 0xFF) {
      processMessage(packet);
      // Send an ACK if the message is directly for this node
      if (packet.dest == myID) {
        sendAck(packet.src, packet.msgID);
      }
    }
    
    // Forward if TTL is positive and message is not exclusively for this node
    if (packet.ttl > 0 && packet.dest != myID) {
      forwardMessage(packet);
    }
  }
}

// Handle button press debouncing and send a message to the specified destination.
void handleButtonPress(int buttonPin, uint8_t destID) {
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
      sendMessage(destID, "Button Pressed from node " + String(myID));
      delay(200); // Prevent multiple triggers
    }
  }
  lastButtonState = reading;
}
