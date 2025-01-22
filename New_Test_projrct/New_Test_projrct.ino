#include <Servo.h>
#include <Keypad.h>
#include <HX711.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <TridentTD_LineNotify.h>
#include <WiFiClient.h>
// กำหนดค่าพารามิเตอร์สำหรับ Line Notify
#define LINE_TOKEN "HVsYN8D7zdUdONK1Pcm9LSQVLKY5Susfj9gG0MtJUsl"

// การเชื่อมต่อ WiFi
const char* SSID = "Somwang";
const char* PASSWORD = "03143681";
WiFiClient espClient;

// การเชื่อมต่อ MQTT
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
char mqtt_Client[] = "cfa54c3d5-205c-4eaa-8695-Project-";
WiFiClient espClient; 
PubSubClient client(espClient);

// กำหนดพิน
#define LOADCELL_DOUT_PIN A0
#define LOADCELL_SCK_PIN A1
#define SERVO_PIN 10

HX711 scale;
Servo myServo;
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Keypad configuration
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {9, 8, 7, 6};
byte colPins[COLS] = {5, 4, 3, 2};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Global variables
float desiredWeight = 0;
bool dispensing = false;
bool foodEmpty = false;
unsigned long lastMsg = 0;

void setup() {
  Serial.begin(115200);

  // WiFi Connection
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

  // Setup MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Servo and HX711 setup
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale();
  scale.tare();
  myServo.attach(SERVO_PIN);
  myServo.write(0);

  // LCD setup
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.print("Welcome:");
  delay(2000);
  lcd.clear();

  // Initialize Line Notify
  LINE.setToken(LINE_TOKEN);
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (client.connect(mqtt_Client)) {
      Serial.println("connected");
      client.subscribe("/project/servo");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  if (String(topic) == "/project/servo") {
    desiredWeight = message.toFloat();
    Serial.print("น้ำหนักที่ตั้งไว้ผ่าน MQTT: ");
    Serial.println(desiredWeight);
    lcd.clear();
    lcd.print("Weight Set: ");
    lcd.print(desiredWeight);
    lcd.print("g");
  }
}

void notifyEmptyFood() {
  LINE.notify("อาหารหมด");
}

void handleKeypadInput(char key) {
  if (key == '*') {
    clearWeight();
  } else if (key == '#') {
    confirmWeight();
  } else if (key >= '0' && key <= '9') {
    enterWeightDigit(key);
  } else if (key == 'A') { // Trigger feeding when button A is pressed
    startFeeding();
  }
}

void startFeeding() {
  if (!dispensing && desiredWeight > 0) {
    myServo.write(45); // เปิด servo เพื่อให้อาหาร
    dispensing = true;
    foodEmpty = false;
    lcd.clear();
    lcd.print("Feeding...");
  }
}

void clearWeight() {
  desiredWeight = 0;
  lcd.clear();
  lcd.print("Weight Cleared");
  delay(1000);
  lcd.clear();
  lcd.print("Set food weight:");
}

void confirmWeight() {
  Serial.print("น้ำหนักที่ตั้งไว้: ");
  Serial.println(desiredWeight);
  lcd.clear();
  lcd.print("Weight Set: ");
  lcd.print(desiredWeight);
  lcd.print("g");
}

void enterWeightDigit(char key) {
  if (desiredWeight < 1000) {
    desiredWeight = desiredWeight * 10 + (key - '0');
    lcd.setCursor(0, 1);
    lcd.print(desiredWeight);
    lcd.print("g   ");
  }
}

void monitorDispensing() {
  float currentWeight = scale.get_units(10);
  if (currentWeight >= desiredWeight) {
    myServo.write(0); // ปิด servo
    dispensing = false;
    lcd.clear();
    lcd.print("Feed Complete");
    scale.tare();
    foodEmpty = false;  // reset status after feeding
  } else if (currentWeight <= 10) { // เช็คว่าอาหารหมด
    foodEmpty = true;
    notifyEmptyFood();
  }
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // อ่านค่าจาก Keypad
  char key = keypad.getKey();
  if (key) {
    handleKeypadInput(key);
  }

  if (dispensing) {
    monitorDispensing();
  }

  // ส่งข้อมูลน้ำหนักที่ตั้งไว้และน้ำหนักปัจจุบันไปยัง Node-RED ทุก ๆ 10 วินาที
  long now = millis();
  if (now - lastMsg > 10000) {
    lastMsg = now;
    float currentWeight = scale.get_units(10);
    String payload = "{\"desiredWeight\":" + String(desiredWeight) + ", \"currentWeight\":" + String(currentWeight) + "}";
    client.publish("/project/weight", payload.c_str());
    Serial.print("Publish message: ");
    Serial.println(payload);
  }

  delay(100);
}
