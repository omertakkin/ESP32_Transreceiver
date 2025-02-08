#ifndef MESH_FUNCTIONS_H
#define MESH_FUNCTIONS_H

#include <Arduino.h>

#define MAX_RECENT_MSG 20
extern uint8_t myID; // Global node identifier (defined in main.cpp)
#define INITIAL_TTL 5

// Define the relay pin (unique name for relay control)
#define RELAY_PIN 27

// Command definitions
#define CMD_OPEN_RELAY   1
#define CMD_CLOSE_RELAY  2
#define CMD_ACK         100  // Special command for ACK packets

extern unsigned long relayStartTime;
extern bool relayActive;
extern const unsigned long RELAY_DURATION;

// New Mesh packet structure with an explicit command field
struct MeshPacket {
  uint32_t msgID;    // Unique message ID for duplicate detection
  uint8_t src;       // Source node ID
  uint8_t dest;      // Destination node ID (or 0xFF for broadcast)
  uint8_t ttl;       // Time-to-live for forwarding
  uint8_t command;   // Command code (e.g., 1 = open relay, 2 = close relay)
  float_t payload;   // Optional additional data
};

// Function prototypes for mesh functions
uint32_t generateUniqueID();
void processMessage(MeshPacket &packet);   // Process received packet based on command
bool isDuplicate(uint32_t id);
void addRecentMsgID(uint32_t id);
void forwardMessage(MeshPacket packet);      // Forward message (TTL is decremented internally)
void sendMessage(uint8_t dest, uint8_t command, float_t payload = 0);
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
void handleButtonPress(int buttonPin, uint8_t destID, uint8_t cmdID);

#endif // MESH_FUNCTIONS_H
