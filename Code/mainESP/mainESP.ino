#include <esp_now.h>
#include <WiFi.h>
#include <SPI.h>
#include <SD.h>
#include <math.h>

const int chipSelect = 5; // Adjust if needed

struct MidiEvent {
  float time;
  char type[10];
  int note;
  int velocity;
};

uint8_t slaveMacs[][6] = {
  {0x8C, 0x4F, 0x00, 0x2E, 0x5E, 0xB0},
  {0x8C, 0x4F, 0x00, 0x2F, 0xF0, 0x4C},
  {0x8C, 0x4F, 0x00, 0x2F, 0x85, 0x1C},
  {0x6C, 0xC8, 0x40, 0x86, 0x28, 0x14},
  {0x6C, 0xC8, 0x40, 0x87, 0xEF, 0x4C},
  {0x6C, 0xC8, 0x40, 0x86, 0xDE, 0xB0}
};

const int numSlaves = sizeof(slaveMacs) / 6;

File midiFile;
MidiEvent currentEvent, nextEvent;

enum State { STATE_WAITING, STATE_RECEIVING, STATE_PLAYING };
State currentState = STATE_WAITING;

bool readNextEvent(MidiEvent &evt) {
  while (true) {
    if (!midiFile.available()) return false;

    String line = midiFile.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue; // Skip empty lines

    int idx1 = line.indexOf(',');
    int idx2 = line.indexOf(',', idx1 + 1);
    int idx3 = line.indexOf(',', idx2 + 1);

    if (idx1 < 0 || idx2 < 0 || idx3 < 0) continue; // Skip malformed lines

    evt.time = line.substring(0, idx1).toFloat();
    line.substring(idx1 + 1, idx2).toCharArray(evt.type, sizeof(evt.type));
    evt.note = line.substring(idx2 + 1, idx3).toInt();
    evt.velocity = line.substring(idx3 + 1).toInt();

    return true;
  }
}

void setup() {
  Serial.begin(115200);
  // while (!Serial) {} // REMOVED: prevents hanging on reset

  // Initialize SD Card
  if (!SD.begin(5, SPI, 4000000)) {
    Serial.println("SD card initialization failed!");
    return;
  }
  Serial.println("SD Card initialized.");

  // Initialize WiFi/ESP-NOW
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Init Failed");
    return;
  }

  for (int i = 0; i < numSlaves; i++) {
    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, slaveMacs[i], 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;

    if (esp_now_add_peer(&peerInfo) != ESP_OK) {
      Serial.printf("Failed to add peer %d\n", i + 1);
    }
  }

  Serial.println("Setup complete. Waiting for CSV via Serial...");
  currentState = STATE_WAITING;
}

void loop() {
  switch (currentState) {
    case STATE_WAITING:
      if (Serial.available()) {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == "START_UPLOAD") {
          Serial.println("Starting upload...");
          // Open file for writing (truncate existing)
          if (SD.exists("/current.csv")) {
            SD.remove("/current.csv");
          }
          midiFile = SD.open("/current.csv", FILE_WRITE);
          if (midiFile) {
             currentState = STATE_RECEIVING;
          } else {
             Serial.println("Error opening file for write");
          }
        }
      }
      break;

    case STATE_RECEIVING:
      if (Serial.available()) {
        String line = Serial.readStringUntil('\n');
        // Check for end signal
        if (line.indexOf("END_UPLOAD") >= 0) {
          Serial.println("Upload complete. Playing...");
          midiFile.close();
          
          // Open for reading
          midiFile = SD.open("/current.csv", FILE_READ);
          if (midiFile) {
             midiFile.readStringUntil('\n'); // skip header
             if (readNextEvent(currentEvent)) {
                currentState = STATE_PLAYING;
             } else {
                Serial.println("Empty file?");
                currentState = STATE_WAITING;
                midiFile.close();
             }
          } else {
             Serial.println("Error opening file for read");
             currentState = STATE_WAITING;
          }
        } else {
          // Write data line
          // Ensure we don't write the start command if it slipped in (unlikely with logic above)
          if (line.indexOf("START_UPLOAD") < 0) {
             midiFile.println(line);
             Serial.println("OK"); // Acknowledge receipt
          }
        }
      }
      break;

    case STATE_PLAYING:
      // Play loop logic
      // Note: This blocks until song finishes because of original logic structure.
      // To make it truly non-blocking would require more complex refactoring,
      // but "Waiting state" requirement is met if we return to it after playing.
      
      while (readNextEvent(nextEvent)) {
        uint8_t note;
        if ((currentEvent.note - 20) % 16 != 0) {
          note = ((currentEvent.note - 20) % 16);
        } else {
          note = 16;
        }
    
        bool isOn = (strcmp(currentEvent.type, "note_on") == 0);
    
        uint8_t data[2] = {
          note,
          isOn
        };
    
        int slaveIndex = floor((currentEvent.note - 21) / 16);
        if (slaveIndex >= 0 && slaveIndex < numSlaves) {
          esp_now_send(slaveMacs[slaveIndex], data, sizeof(data));
        }
    
        int delayMs = max(0, int((nextEvent.time - currentEvent.time) * 1000));
        delay(delayMs);
    
        currentEvent = nextEvent;
      }
      
      // Song finished
      Serial.println("Song finished. Saving power/silence...");
      
      // Note OFF all keys
      for (int note = 21; note <= 108; note++) {
        uint8_t mappedNote = ((note - 20) % 16 != 0) ? ((note - 20) % 16) : 16;
        uint8_t isOn = 0; 
        uint8_t data[2] = { mappedNote, isOn };
    
        int slaveIndex = floor((note - 21) / 16);
        if (slaveIndex >= 0 && slaveIndex < numSlaves) {
          esp_now_send(slaveMacs[slaveIndex], data, sizeof(data));
        }
      }

      midiFile.close();
      currentState = STATE_WAITING;
      Serial.println("Waiting for next song...");
      break;
  }
}
