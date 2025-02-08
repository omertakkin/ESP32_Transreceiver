#ifndef MESH_FUNCTIONS_H
#define MESH_FUNCTIONS_H

#include <Arduino.h>

// --- Global Definitions ---
#define MAX_RECENT_MSG 20
extern uint8_t myID; // Global node identifier (defined in main.cpp)
#define INITIAL_TTL 5

// Define the relay pin and duration (use a unique name for the relay pin)
#define RELAY_PIN 27
extern unsigned long relayStartTime;
extern bool relayActive;
extern const unsigned long RELAY_DURATION;

// --- Function Prototypes for Mesh Functions ---
uint32_t generateUniqueID();
void processMessage(String payload, uint32_t msgID, uint8_t src, uint8_t dest, uint8_t ttl);
bool isDuplicate(uint32_t id);
void addRecentMsgID(uint32_t id);
void forwardMessage(uint32_t msgID, uint8_t src, uint8_t dest, uint8_t ttl, String payload);
void sendMessage(uint8_t dest, String payload);
void sendAck(uint8_t dest, uint32_t msgID);

// --- Relay Functions ---
void openRelay();   // Activate the relay and record start time.
void closeRelay();  // Deactivate the relay.
void updateRelay(); // Check if the relay duration has elapsed and then close it.

#endif  // MESH_FUNCTIONS_H
