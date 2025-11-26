#include "arduino_secrets.h"


#include <ArduinoIoTCloud.h>
#include <Arduino_ConnectionHandler.h>
#include "thingProperties.h"

#define RXD2 16
#define TXD2 17
#define RELAY_PIN 5
#define SOIL_PIN 34   // Single soil sensor input

byte npkRequest[] = {0x01,0x03,0x00,0x1E,0x00,0x03,0xE5,0xCC};

void setup() {
  Serial.begin(9600);
  delay(1500);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Motor OFF at startup

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

  setDebugMessageLevel(2);
  ArduinoCloud.printDebugInfo();
}

void loop() {
  ArduinoCloud.update();

  // --- Read NPK Sensor via RS485 ---
  Serial2.write(npkRequest, sizeof(npkRequest));
  delay(200);

  if (Serial2.available() >= 11) {
    byte response[11];
    for (int i = 0; i < 11; i++) {
      response[i] = Serial2.read();
    }

    uint16_t nitrogen   = (uint16_t)response[3] << 8 | response[4];
    uint16_t phosphorus = (uint16_t)response[5] << 8 | response[6];
    uint16_t potassium  = (uint16_t)response[7] << 8 | response[8];

    // - Clamp to realistic ranges -
    nitrogen   = constrain(map(nitrogen, 0, 65535, 0, 500), 0, 500);
    phosphorus = constrain(map(phosphorus, 0, 65535, 0, 300), 0, 300);
    potassium  = constrain(map(potassium, 0, 65535, 0, 800), 0, 800);

    // - Classify nutrient levels -
    String nStatus = (nitrogen < 280) ? "Low" : (nitrogen <= 450 ? "Correct" : "High");
    String pStatus = (phosphorus < 22) ? "Low" : (phosphorus <= 55 ? "Correct" : "High");
    String kStatus = (potassium < 110) ? "Low" : (potassium <= 280 ? "Correct" : "High");

    // - Update Dashboard String -
    nPKSENSOR = "N=" + String(nitrogen) + "(" + nStatus + "), " +
                "P=" + String(phosphorus) + "(" + pStatus + "), " +
                "K=" + String(potassium) + "(" + kStatus + ")";

    Serial.println(nPKSENSOR);
  }

  // - Soil Moisture -
  int sensorValue = analogRead(SOIL_PIN);
  int moisturePercent = map(sensorValue, 0, 4095, 100, 0);
  soilmoisture = moisturePercent;

  Serial.print("Soil Moisture = ");
  Serial.print(moisturePercent);
  Serial.println("%");

  // - Motor Control Logic -
  //   // Turn ON when dry (< 40%), Turn OFF when wet (> 70%)
  if (moisturePercent < 40 && !motor) {
    motor = true;
    digitalWrite(RELAY_PIN, LOW);  // Relay ON
    Serial.println("Soil is Dry → Motor ON");
  } 
  else if (moisturePercent > 70 && motor) {
    motor = false;
    digitalWrite(RELAY_PIN, HIGH); // Relay OFF
    Serial.println("Soil is Wet → Motor OFF");
  }

  delay(2000); // keeps loop stable
}

void onMotorChange() {
  if (motor) {
    digitalWrite(RELAY_PIN, LOW);
    Serial.println("Motor ON (Cloud)");
  } else {
    digitalWrite(RELAY_PIN, HIGH);
    Serial.println("Motor OFF (Cloud)");
  }
}

void onNPKSENSORChange() {
  // Called when Cloud edits the string
}

void onSoilmoistureChange() {
  // Optional usage
}
