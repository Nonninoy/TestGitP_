#include <ESP32Servo.h>
#include <Keypad.h>
#include <HX711.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);

#include <TridentTD_LineNotify.h>
#define LINE_TOKEN "HVsYN8D7zdUdONK1Pcm9LSQVLKY5Susfj9gG0MtJUsl"

const byte ROWS = 4; //four rows
const byte COLS = 4; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
byte rowPins[ROWS] = {34, 35, 32, 33};  //D7-D4
byte colPins[COLS] = {25, 26, 27,14}; //D2-D0
Keypad keypad = Keypad( makeKeymap(keys), rowPins, colPins, ROWS, COLS );

// กำหนดพินของ HX711
#define LOADCELL_DOUT_PIN  18
#define LOADCELL_SCK_PIN   19

#define SDA_PIN 21
#define SCL_PIN 22

HX711 scale;
Servo myServo;
#define servoPin 4
RTC_DS3231 rtc;
scale.set_scale();  // กำหนดค่า scale เริ่มต้น
scale.tare();       // ตั้งค่าเริ่มต้นให้เป็น 0

float desiredWeight = 0.0;
int feedingTimes[4] = {0}; // สำหรับเก็บเวลาให้อาหาร 4 ช่วง
bool dispensing = false;
byte weightIndex = 0;

char SSID[] = "Somwang";
char PASSWORD[] = "03143681";  

const char* mqtt_server = "broker.hivemq.com"; 
const int mqtt_port = 8883;

//const char* mqtt_topic = "test/topic";

const char* mqtt_Client = "cfa54c3d5-205c-4eaa-8695-Project-";  
const char* mqtt_username = "";  
const char* mqtt_password = "";

// สร้าง WiFi client สำหรับ TLS
WiFiClientSecure espClient;
PubSubClient client(espClient);

int led = 17;

void setup() {
  Serial.begin(115200);
  // start
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.print("Set food weight:");
  delay(2000);
  lcd.clear();
  
  //loadcell
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  scale.set_scale();
  scale.tare();

  // line แจ้งเตือน
  LINE.setToken(LINE_TOKEN);

  //เวลา
  Wire.begin();
  rtc.begin();
  
  // Connect to WiFi
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");
  client.setCallback(callback); //ตั้งค่าฟังก์ชันที่จะทำงานเมื่อมีข้อมูลเข้ามาผ่านการ Subscribe
  client.subscribe("/project");

  myServo.attach(servoPin, 500, 2400); // servo อาหารแมว

}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
  if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) { //เชื่อมต่อกับ MQTT BROKER
    Serial.println("connected");
    client.subscribe("/setting");
  }
  else {
    Serial.print("failed, rc=");
    Serial.print(client.state());
    Serial.println("try again in 5 seconds");
    delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  //Serial.print("Message arrived [");
  //Serial.print(topic);
  //Serial.print("] ");
  for (int i = 0; i < length; i++) {
    message = message + char(payload[i]);
  }
  //Serial.println(message);
  if(Stringi(topic) == "/setting") {
     if (message == "ON"){
        checkFeedingTime();
        monitorDispensing();
        }
  }
}

void notify(){
  LINE.notify("Feeding!");
}

void handleKeypadInput(char key) {
  if (key == '*') { // เคลียร์น้ำหนัก
    clearWeight();
  } else if (key == '#') { // ยืนยันน้ำหนักที่ตั้งไว้
    confirmWeight();
  } else if (key == 'A') { // ตั้งค่ามื้ออาหารที่ 1
    setFeedingTime(0);
  } else if (key == 'B') { // ตั้งค่ามื้ออาหารที่ 2
    setFeedingTime(1);
  } else if (key == 'C') { // ตั้งค่ามื้ออาหารที่ 3
    setFeedingTime(2);
  } else if (key == 'D') { // ตั้งค่ามื้ออาหารที่ 4
    setFeedingTime(3);
  } else if (key >= '0' && key <= '9') { // ป้อนตัวเลขน้ำหนัก
    enterWeightDigit(key);
  }
}

void handleKeypadInput(char key) {
  if (key == '*') { // เคลียร์น้ำหนัก
    clearWeight();
  } else if (key == '#') { // ยืนยันน้ำหนักที่ตั้งไว้
    confirmWeight();
  } else if (key == 'A') { // ตั้งค่ามื้ออาหารที่ 1
    setFeedingTime(0);
  } else if (key == 'B') { // ตั้งค่ามื้ออาหารที่ 2
    setFeedingTime(1);
  } else if (key == 'C') { // ตั้งค่ามื้ออาหารที่ 3
    setFeedingTime(2);
  } else if (key == 'D') { // ตั้งค่ามื้ออาหารที่ 4
    setFeedingTime(3);
  } else if (key >= '0' && key <= '9') { // ป้อนตัวเลขน้ำหนัก
    enterWeightDigit(key);
  }
}

void clearWeight() {
  desiredWeight = 0;
  weightIndex = 0;

  lcd.clear();
  lcd.print("Weight Cleared");
  delay(1000);
  lcd.clear();
  lcd.print("Set food weight:");
}

void confirmWeight() {
  Serial.print("น้ำหนักที่กำหนด: ");
  Serial.print(desiredWeight);
  Serial.println(" กรัม");
  lcd.clear();
  lcd.print("Weight Set: ");
  lcd.print(desiredWeight);
  lcd.print("g");

  checkFeedingTime();
  weightIndex = 0;
}

void enterWeightDigit(char key) {
  // รีเซ็ตน้ำหนักหากมีการกด '*'
  if (weightIndex < 3) {
    lcd.setCursor(weightIndex, 1);
    lcd.print(key);
    desiredWeight = desiredWeight * 10 + (key - '0');
    weightIndex++;

    // แสดงน้ำหนักที่กำหนดในบรรทัดที่ 2
    lcd.setCursor(0, 1);
    lcd.print(desiredWeight);
    lcd.print("g    "); // เคลียร์ค่าเก่าออก
  }
}

void setFeedingTime(int meal) {
  DateTime now = rtc.now();
  feedingTimes[meal] = now.hour() * 100 + now.minute();

  Serial.print("มื้ออาหารที่ ");
  Serial.print(meal + 1);
  Serial.print(": ");
  Serial.print(feedingTimes[meal]);
  Serial.println(" ตั้งเวลาเรียบร้อยแล้ว");

  lcd.clear();
  lcd.print("Time ");
  lcd.print(meal + 1);
  lcd.print(": ");
  lcd.print(feedingTimes[meal]);
}

void checkFeedingTime() {
  DateTime now = rtc.now();
  int currentTime = now.hour() * 100 + now.minute();

  for (int i = 0; i < 4; i++) {
    if (currentTime == feedingTimes[i] && desiredWeight > 0) {
      myServo.write(90); // เปิด servo
      dispensing = true;
      Serial.print("ให้อาหารมื้อที่ ");
      Serial.print(i + 1);
      Serial.println(" เริ่มการให้อาหาร");

      lcd.clear();
      lcd.print("Feeding ");
      lcd.print(i + 1);
      break;
    }
  }
}

void monitorDispensing() {
  float currentWeight = scale.get_units(10);
  if (currentWeight >= desiredWeight) {
    myServo.write(0); // ปิด servo
    dispensing = false;
    Serial.println("การให้อาหารเสร็จสิ้น");

    lcd.clear();
    lcd.print("Feed Complete");
    scale.tare();
  }
}
void loop() {

  // อ่านค่าจาก Keypad
  char key = keypad.getKey();

  if (key) {
    handleKeypadInput(key);
  }

  if (dispensing) {
    monitorDispensing();
  }
  delay(100);
  
  client.loop();
  long now = millis();
  if (now - lastMsg > 10000) { //จับเวลาส่งข้อมูลทุก ๆ 10 วินาที
    lastMsg = now;
    ++value;
    int h = desiredWeight(); //น้ำหนักที่เซต
    int t = currentWeight(); //น้ำหนักอาหาร loadcell
    DataString = "{\"data\":{\"desiredWeight\":"+(String)t+",\}"; 
    // Example of data : {"data":{"temperature":25 , "humidity": 60}}
    DataString.toCharArray(msg, 100);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client.publish("/Value",msg); //R อย่าลืมแก้ไข TOPIC ที่จะทำการ PUBLISH ไปยัง MQTT BROKE
  }
}
