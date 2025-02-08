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

// Generate a unique message ID by combining the node's ID and a truncated millis() value.
uint32_t generateUniqueID() {
  return ((uint32_t)myID << 24) | (millis() & 0x00FFFFFF);
}

/*
 * @brief Processes incoming mesh network commands and executes actions.
 * 
 * @param packet Reference to a MeshPacket containing the command and source node.
 * 
 * This function reads the command from the received packet, logs it to the serial monitor,  
 * and executes the corresponding action (opening or closing a relay).  
 * If the command is unknown, it logs an error message.
 */
void processMessage(MeshPacket &packet) {
  Serial.print("Processing command ");
  Serial.print(packet.command);
  Serial.print(" from node ");
  Serial.println(packet.src);
  
  switch (packet.command) {
    case CMD_OPEN_RELAY:
      openRelay();
      break;
    case CMD_CLOSE_RELAY:
      closeRelay();
      break;
    default:
      Serial.print("Unknown command: ");
      Serial.println(packet.command);
      break;
  }
}

/*
 * @brief Checks if a message ID has already been processed.
 * 
 * @param id Unique identifier of the message.
 * @return true if the message ID is found in recentMsgIDs, false otherwise.
 * 
 * This function iterates through the recent message IDs and returns true if  
 * the given ID matches any stored entry, indicating a duplicate message.
 */
bool isDuplicate(uint32_t id) {
  for (int i = 0; i < recentMsgCount; i++) {
    if (recentMsgIDs[i] == id)
      return true;
  }
  return false;
}

/*
 * @brief Adds a message ID to the recent messages cache.
 * 
 * @param id Unique identifier of the message.
 * 
 * If the cache is not full, the ID is added to the next available slot.  
 * If the cache is full, it shifts all stored IDs (FIFO) to make space for the new one.
 */
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

/*
 * @brief Forwards a message by decrementing TTL and re-transmitting it.
 * 
 * @param packet The MeshPacket to be forwarded.
 * 
 * If the packet's TTL (Time-To-Live) is greater than zero, it is decremented,  
 * serialized into a string, and transmitted using LoRa.
 */
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

/*
 * @brief Sends a command message by creating, serializing, and transmitting a MeshPacket.
 * 
 * @param dest Destination node ID.
 * @param command Command code to be sent.
 * @param payload Floating-point payload data.
 * 
 * This function constructs a MeshPacket with a unique ID, source, destination, TTL,  
 * command, and payload. The packet is serialized and transmitted using LoRa,  
 * and its message ID is stored in the recent messages cache.
 */
void sendMessage(uint8_t dest, uint8_t command, float_t payload) {
  MeshPacket packet;
  packet.msgID   = generateUniqueID();
  packet.src     = myID;
  packet.dest    = dest;
  packet.ttl     = INITIAL_TTL;
  packet.command = command;
  packet.payload = payload;

  String msg = serializePacket(packet);
  Serial.print("Sending message: ");
  Serial.println(msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  addRecentMsgID(packet.msgID);
}

/*
 * @brief Sends an acknowledgment (ACK) packet back to the sender.
 * 
 * @param dest Destination node ID (original sender).
 * @param msgID Message ID of the original packet being acknowledged.
 * 
 * This function constructs an ACK MeshPacket using the original message ID,  
 * sets a minimal TTL, and transmits it using LoRa.
 */
void sendAck(uint8_t dest, uint32_t msgID) {
  MeshPacket packet;
  packet.msgID   = msgID; // Use the original message ID to correlate
  packet.src     = myID;
  packet.dest    = dest;
  packet.ttl     = 1;      // Minimal TTL for ACK
  packet.command = CMD_ACK;
  packet.payload = 0.0;

  String msg = serializePacket(packet);
  Serial.print("Sending ACK: ");
  Serial.println(msg);
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
}

// --- Packet Serialization Functions ---

/*
 * @brief Serializes a MeshPacket into a CSV-formatted string.
 * 
 * @param packet Reference to the MeshPacket to be serialized.
 * @return A CSV string in the format: msgID,src,dest,ttl,command,payload
 * 
 * This function converts the packet's fields into a comma-separated string,  
 * making it suitable for transmission or logging.
 */
String serializePacket(const MeshPacket &packet) {
  return String(packet.msgID) + "," +
         String(packet.src) + "," +
         String(packet.dest) + "," +
         String(packet.ttl) + "," +
         String(packet.command) + "," +
         packet.payload;
}

/*
 * @brief Deserializes a CSV-formatted string into a MeshPacket.
 * 
 * @param data The CSV string containing packet data.
 * @return A MeshPacket populated with parsed values. If the input is malformed,  
 *         a default-initialized packet is returned.
 * 
 * This function extracts values from a CSV string using comma indexes,  
 * converts them into appropriate data types, and assigns them to a MeshPacket.
 */
MeshPacket deserializePacket(const String &data) {
  MeshPacket packet;
  int idx1 = data.indexOf(','); // msgID
  int idx2 = data.indexOf(',', idx1 + 1); // src
  int idx3 = data.indexOf(',', idx2 + 1); // dest
  int idx4 = data.indexOf(',', idx3 + 1); // ttl
  int idx5 = data.indexOf(',', idx4 + 1); // command
  
  if (idx1 == -1 || idx2 == -1 || idx3 == -1 || idx4 == -1 || idx5 == -1) {
    Serial.println("Malformed packet");
    return packet; // Returns a packet with default values.
  }
  
  packet.msgID   = data.substring(0, idx1).toInt();
  packet.src     = (uint8_t)data.substring(idx1 + 1, idx2).toInt();
  packet.dest    = (uint8_t)data.substring(idx2 + 1, idx3).toInt();
  packet.ttl     = (uint8_t)data.substring(idx3 + 1, idx4).toInt();
  packet.command = (uint8_t)data.substring(idx4 + 1, idx5).toInt();
  packet.payload = (float_t)data.substring(idx5 + 1).toFloat();
  
  return packet;
}

// --- Relay Control Functions ---

/*
 * @brief Activates the relay and starts the timer.
 * 
 * This function turns on the relay, records the activation time,  
 * and sets a flag indicating the relay is active.
 */
void openRelay() {
  relayStartTime = millis();
  relayActive = true;
  digitalWrite(RELAY_PIN, HIGH);
  Serial.println("Relay On");
}

/*
 * @brief Deactivates the relay.
 * 
 * This function turns off the relay and clears the active flag.
 */
void closeRelay() {
  relayActive = false;
  digitalWrite(RELAY_PIN, LOW);
  Serial.println("Relay Off");
}

/*
 * @brief Checks if the relay duration has elapsed and turns it off if needed.
 * 
 * This function continuously monitors the relay state.  
 * If the relay has been active for longer than RELAY_DURATION, it deactivates it.
 */
void updateRelay() {
  if (relayActive && (millis() - relayStartTime >= RELAY_DURATION)) {
    closeRelay();
  }
}

// --- Incoming Packet and Button Handling ---

/*
 * @brief Handles incoming LoRa packets: deserializes, checks for duplicates, 
 *        processes commands, sends acknowledgments, and forwards if necessary.
 * 
 * This function listens for incoming LoRa packets, deserializes them into a MeshPacket,  
 * checks for duplicates, processes the message based on the command, and sends an ACK  
 * if the message is intended for the local node. If TTL permits, the packet is forwarded.
 */
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
    
    // If the message is for this node (or broadcast), process it.
    if (packet.dest == myID || packet.dest == 0xFF) {
      processMessage(packet);
      // Send an ACK if the message is directly for this node and isn’t an ACK itself.
      if (packet.dest == myID && packet.command != CMD_ACK) {
        sendAck(packet.src, packet.msgID);
      }
    }
    
    // Forward the packet if TTL remains and the message isn’t exclusively for this node.
    if (packet.ttl > 0 && packet.dest != myID) {
      forwardMessage(packet);
    }
  }
}


/*
 * @brief Handles button presses with debounce and sends a command message.
 * 
 * @param buttonPin The pin where the button is connected.
 * @param destID The destination node ID to send the command to.
 * @param cmdID The command ID to be sent.
 * 
 * This function checks for a button press with debounce, and once the button is pressed,  
 * it sends a command message (e.g., to open a relay) to the specified destination node.  
 * A delay is added to prevent multiple triggers due to button bounce.
 */
void handleButtonPress(int buttonPin, uint8_t destID, uint8_t cmdID) {
  static bool lastButtonState = HIGH;
  bool reading = digitalRead(buttonPin);
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50;
  
  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW) {
      Serial.print("Button pressed, sending command to node ");
      Serial.println(destID);
      // Example: send an "open relay" command using CMD_OPEN_RELAY
      sendMessage(destID, cmdID, 0);
      delay(200); // Prevent multiple triggers
    }
  }
  lastButtonState = reading;
}
