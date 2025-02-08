#include <SPI.h>
#include <LoRa.h>
#include "MeshFunctions.h"

// Define LoRa and SPI pins (adjust as needed)
#define SS_PIN    5
#define RST_PIN   14
#define DIO0_PIN  2
#define SCK_PIN   18
#define MISO_PIN  17
#define MOSI_PIN  23

// Define the button pin (for initiating transmissions)
const int buttonPin = 26;

// Global node identifier (adjust per device)
uint8_t myID = 1;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("LoRa Mesh Node Starting...");

  // Initialize button pin and relay pin (RELAY_PIN is defined in MeshFunctions.h as 27)
  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Ensure relay is off initially

  // Initialize SPI with custom pins
  SPI.begin(SCK_PIN, MISO_PIN, MOSI_PIN, SS_PIN);

  // Set LoRa pins and initialize LoRa at the chosen frequency (e.g., 433E6)
  LoRa.setPins(SS_PIN, RST_PIN, DIO0_PIN);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check your connections.");
    while (true);
  }
  Serial.println("LoRa init succeeded.");
}

void loop() {
  // Check for and process any incoming LoRa packets
  handleIncomingPacket();

  // Update relay status (turn off relay after its duration has elapsed)
  updateRelay();

  // Check for button press and send message to node with ID 0x01 (or any other value you want)
  handleButtonPress(buttonPin , 0x02, CMD_OPEN_RELAY);
}
