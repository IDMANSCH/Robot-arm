/*
 * ESP32 TRANSMITTER - Bluetooth Serial Remote Control
 * Uses 6 potentiometers to send data to the Receiver via Bluetooth
 */

#include <BluetoothSerial.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

BluetoothSerial SerialBT;
// Bluetooth device name
const char* deviceName = "RobotArm_Controller";

// Data structure for transmission
typedef struct struct_message {
  uint16_t servo1;   // 0-180
  uint16_t servo2;   // 0-180
  uint16_t servo3;   // 0-180
  uint16_t servo4;   // 0-180
  uint16_t servo5;   // 0-180
  int16_t stepper;   // -100 đến 100
  uint8_t sequence;  // Packet sequence number
} struct_message;

struct_message controlData;

// Potentiometer pins
const int potPins[6] = { 13, 14, 27, 26, 25, 33 };
int potValues[6] = { 0 };
int lastPotValues[6] = { 0 };

// Bluetooth connection status
bool isConnected = false;
unsigned long lastConnectionCheck = 0;
const int CONNECTION_CHECK_INTERVAL = 2000;  // ms

// Timing control variables
unsigned long lastSendTime = 0;
const int SEND_INTERVAL = 100;  // ms
uint8_t sequenceNum = 0;

void setup() {
  Serial.begin(115200);

  // Initialize Bluetooth Serial
  SerialBT.begin(deviceName, true);
  Serial.println("Bluetooth Serial đã khởi tạo");
  Serial.print("Tên thiết bị: ");
  Serial.println(deviceName);
  Serial.print("Đang chờ kết nối...");
  while (!SerialBT.connect("RobotArm_Receiver")) {
    Serial.print(".");
    delay(1000);
  }

  // Configure analog pins
  for (int i = 0; i < 6; i++) {
    pinMode(potPins[i], INPUT);
    lastPotValues[i] = analogRead(potPins[i]);
  }

  Serial.println("Transmitter ESP-NOW sẵn sàng");
}

void loop() {
  checkBluetoothConnection();

  if (isConnected) {
    // Read potentiometer values
    readPotentiometers();

    // Send data periodically
    if (millis() - lastSendTime >= SEND_INTERVAL) {
      prepareData();
      sendData();
      lastSendTime = millis();
    }
  } else {
    // Read and display potentiometer values when disconnected (debug)
    if (millis() - lastSendTime >= 1000) {
      readPotentiometers();
      prepareData();
      Serial.print("Chưa kết nối - Giá trị: ");
      Serial.print(controlData.servo1);
      Serial.print(", ");
      Serial.print(controlData.servo2);
      Serial.print(", ");
      Serial.print(controlData.servo3);
      Serial.print(", ");
      Serial.print(controlData.servo4);
      Serial.print(", ");
      Serial.print(controlData.servo5);
      Serial.print(" | Stepper: ");
      Serial.println(controlData.stepper);
      lastSendTime = millis();
    }
  }

  delay(1);  // Reduce CPU usage
}

void checkBluetoothConnection() {
  if (millis() - lastConnectionCheck >= CONNECTION_CHECK_INTERVAL) {
    isConnected = SerialBT.hasClient();
    if (isConnected) {
      Serial.println("Đã kết nối Bluetooth!");
    } else {
      Serial.println("Đang chờ kết nối Bluetooth...");
    }
    lastConnectionCheck = millis();
  }
}

void readPotentiometers() {
  for (int i = 0; i < 6; i++) {
    int rawValue = analogRead(potPins[i]);

    // Noise filtering using moving average
    if (abs(rawValue - lastPotValues[i]) > 8 || abs(rawValue - lastPotValues[i]) < 30) {
      lastPotValues[i] = rawValue;
      potValues[i] = rawValue;
    } else {
      potValues[i] = lastPotValues[i];
    }

    // Debug
    Serial.print(potValues[i]);
    Serial.print(", ");
  }
  Serial.println();
}

void prepareData() {
  // Map the first 5 potentiometers to servo values (0-180)
  for (int i = 0; i < 5; i++) {
    int value = map(potValues[i], 0, 4095, 0, 180);
    value = constrain(value, 0, 180);

    switch (i) {
      case 0: controlData.servo1 = value; break;
      case 1: controlData.servo2 = value; break;
      case 2: controlData.servo3 = value; break;
      case 3: controlData.servo4 = value; break;
      case 4: controlData.servo5 = value; break;
    }
  }

  // Sixth potentiometer controls the stepper (-100-100)
  int stepperVal = map(potValues[5], 0, 4095, 0, 270);
  stepperVal = constrain(stepperVal, 0, 270);
  controlData.stepper = stepperVal;

  // Increment packet sequence number
  controlData.sequence = sequenceNum++;
  if (sequenceNum > 255) {
    sequenceNum = 0;
  }

  // Debug output
  Serial.print("Servo: ");
  Serial.print(controlData.servo1);
  Serial.print(", ");
  Serial.print(controlData.servo2);
  Serial.print(", ");
  Serial.print(controlData.servo3);
  Serial.print(", ");
  Serial.print(controlData.servo4);
  Serial.print(", ");
  Serial.print(controlData.servo5);
  Serial.print(" | Stepper: ");
  Serial.print(controlData.stepper);
  Serial.print(" | Seq: ");
  Serial.println(controlData.sequence);
}

void sendData() {
  if (!isConnected) return;

  // Send data as a JSON string for easier processing
  String dataString = "{";
  dataString += "\"s1\":" + String(controlData.servo1) + ",";
  dataString += "\"s2\":" + String(controlData.servo2) + ",";
  dataString += "\"s3\":" + String(controlData.servo3) + ",";
  dataString += "\"s4\":" + String(controlData.servo4) + ",";
  dataString += "\"s5\":" + String(controlData.servo5) + ",";
  dataString += "\"st\":" + String(controlData.stepper) + ",";
  dataString += "\"seq\":" + String(controlData.sequence);
  dataString += "}\n";

  SerialBT.print(dataString);

  // Debug output
  Serial.print("Gửi: ");
  Serial.print(dataString);
}

