#ifndef MESH_FUNCTIONS_H
#define MESH_FUNCTIONS_H

#include <Arduino.h>

#define MAX_RECENT_MSG 20
extern uint8_t myID; // Global node identifier (defined in main.cpp)
#define INITIAL_TTL 5

// Define the relay pin (unique name for relay control)
#define RELAY_PIN 27

extern unsigned long relayStartTime;
extern bool relayActive;
extern const unsigned long RELAY_DURATION;

// New Mesh packet structure
struct MeshPacket {
  uint32_t msgID;
  uint8_t src;
  uint8_t dest;
  uint8_t ttl;
  String payload;
};

// Function prototypes for mesh functions
uint32_t generateUniqueID();
void processMessage(MeshPacket &packet);
bool isDuplicate(uint32_t id);
void addRecentMsgID(uint32_t id);
void forwardMessage(MeshPacket packet);  // Forwarding will decrement TTL internally
void sendMessage(uint8_t dest, String payload);
void sendAck(uint8_t dest, uint32_t msgID);

// Packet serialization and deserialization functions
String serializePacket(const MeshPacket &packet);
MeshPacket deserializePacket(const String &data);

// Relay control functions
void openRelay();
void closeRelay();
void updateRelay();

// Helper functions for handling transmissions and receptions
void handleIncomingPacket();
void handleButtonPress(int buttonPin, uint8_t destID);

#endif // MESH_FUNCTIONS_H
