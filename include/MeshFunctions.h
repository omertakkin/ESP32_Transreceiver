#ifndef MESH_FUNCTIONS_H
#define MESH_FUNCTIONS_H

#include <Arduino.h>
#include <LoRa.h>

// Configuration Constants
#define MAX_RECENT_MSG 20
#define INITIAL_TTL 5
#define RELAY_PIN 27
#define RELAY_DURATION 1000
#define MAX_PENDING_MSGS 5
#define ACK_TIMEOUT 2000
#define MAX_RETRIES 3
#define LORA_TIMEOUT_MS 3000

// Command Definitions
#define CMD_OPEN_RELAY   1
#define CMD_CLOSE_RELAY  2
#define CMD_ACK          100

#pragma pack(push, 1)
struct MeshPacket {
  uint32_t msgID;
  uint8_t src;
  uint8_t dest;
  uint8_t ttl;
  uint8_t command;
  float payload;
};
#pragma pack(pop)

struct PendingMessage {
  uint32_t msgID;
  uint8_t dest;
  uint8_t command;
  float payload;
  unsigned long sendTime;
  uint8_t retries;
};

// Extern Variables
extern uint8_t myID;
extern unsigned long relayStartTime;
extern bool relayActive;
extern PendingMessage pendingMessages[];
extern uint8_t pendingCount;

// Core Mesh Functions
void processMessage(MeshPacket &packet);
void forwardMessage(MeshPacket packet);
void sendMessage(uint8_t dest, uint8_t command, float_t payload = 0);
void sendAck(uint8_t dest, uint32_t msgID);

// Packet Management
void serializePacket(const MeshPacket &packet, uint8_t* buffer);
void deserializePacket(const uint8_t* buffer, MeshPacket &packet);
bool isDuplicate(uint32_t id);
void addRecentMsgID(uint32_t id);

// System Functions
void checkPendingAcks();
void handleIncomingPacket();
void handleButtonPress(int buttonPin, uint8_t destID, uint8_t cmdID);

// Relay Control
void openRelay();
void closeRelay();
void updateRelay();

#endif