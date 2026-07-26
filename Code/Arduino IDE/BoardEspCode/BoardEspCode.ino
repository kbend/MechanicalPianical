#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <Stepper.h> 
const int stepsPerRevolution = -200;
Stepper myStepper = Stepper(stepsPerRevolution, 2, 4, 5, 19);

const int numNotes = 16;
const int outputPins[numNotes] = {
  32, 33, 25, 26, 27, 14, 12, 13,
  23, 22, 21, 19, 18, 17, 16, 4
};

void onDataReceive(const esp_now_recv_info_t *recvInfo, const uint8_t *incomingData, int len) {
  if (len != 2) {
    Serial.printf("Received %d bytes, expected 4\n", len);
    return;
  }

  uint8_t note = incomingData[0];
  bool isOn = incomingData[1];
  
  if (note >= 1 && note <= numNotes) {
    digitalWrite(outputPins[note - 1], isOn ? HIGH : LOW);
  } else {
    Serial.printf("Invalid note %d\n", note);
  }

  if (note == 100){
    invertSign(stepsPerRevolution);
    myStepper.step(stepsPerRevolution);
  }
}

void invertSign(value) {
  return value * -1;
}


void setup() {
  Serial.begin(115200);
  myStepper.setSpeed(60);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  // 🔹 Get and display this ESP32's MAC address
  Serial.print("Device MAC Address: ");
  Serial.println(WiFi.macAddress());

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed!");
    return;
  }

  esp_now_register_recv_cb(onDataReceive);
  Serial.println("ESP-NOW Receiver Ready! Waiting for messages...");

  for (int value : outputPins){
    pinMode(value, OUTPUT);
  }
}

void loop() {
  // put your main code here, to run repeatedly:
}
