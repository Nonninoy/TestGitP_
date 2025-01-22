#include <WiFi.h>
#include <Servo.h>
#include <TridentTD_LineNotify.h>

#define LINE_TOKEN "HVsYN8D7zdUdONK1Pcm9LSQVLKY5Susfj9gG0MtJUsl"
char SSID[] = "Somwang";
char PASSWORD[] = "03143681";

int led1 = 4;
int servoPin = 2;
int angle = 0; 
#define button 5

int t = 100;

Servo myServo; // Create a Servo object

void Wifi_begin(){
  WiFi.begin(SSID,PASSWORD);
  LINE.setToken(LINE_TOKEN);
}

void setup() {
  Serial.begin(115200);
  pinMode(led1,OUTPUT);
  pinMode(LED_BUILTIN,OUTPUT);
  pinMode(button,INPUT_PULLUP);

  Wifi_begin(); // Call WiFi initialization once

  // Initialize the servo on pin 2
  myServo.attach(servoPin);
}

void loop() {
  digitalWrite(LED_BUILTIN, HIGH); 
  delay(10);
  digitalWrite(LED_BUILTIN, LOW); 
  delay(10);

  int button_state = digitalRead(button);
  Serial.print("button_state : ");
  Serial.println(button_state);
  delay(t);

  if(button_state == LOW) {
    digitalWrite(led1, 1);
    delay(t);
    digitalWrite(led1, 0);
    delay(t);
    LINE.notify("button is push");
    myServo.write(180);
    delay(1000);
    myServo.write(0);
    delay(5000);
  }
}