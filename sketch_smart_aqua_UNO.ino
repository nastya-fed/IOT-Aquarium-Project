#include "SoftwareSerial.h"
#include <Servo.h>
#include <math.h>

SoftwareSerial ESP(2, 3);  // RX, TX для связи с ESP-01

// Лампочка
const int lampPin = 4;

// Сервопривод
Servo Servopriv;
const int servoPin = 5;

//Термистор
#define B 3950
#define SERIAL_R 4700
#define THERMISTOR_R 10000
#define NOMINAL_T 25
const byte tempPin = A0;

void setup() {

  Serial.begin(9600);
  ESP.begin(9600);
  delay(500);

  pinMode(lampPin, OUTPUT);
  digitalWrite(lampPin, LOW);

  Servopriv.attach(servoPin);
  Servopriv.write(0);
}

void loop() {

  if (ESP.available()) {

    String cmd = ESP.readStringUntil('\n');
    cmd.trim();


    if (cmd.startsWith("CMD:")) {

      if (cmd == "CMD:ON") {

        digitalWrite(lampPin, HIGH);
        Serial.println("Лампочка вкл");

      } else if (cmd == "CMD:OFF") {

        digitalWrite(lampPin, LOW);
        Serial.println("Лампочка выкл");

      } else if (cmd == "CMD:FEED") {

        Servopriv.write(180);
        delay(2000);
        Servopriv.write(0);
        Serial.println("Кормление");

      } else if (cmd == "CMD:TEMP") {

        int t = analogRead(tempPin);
        float tr = 1023.0 / t - 1;
        tr = SERIAL_R / tr;  // формула Стейнхарта–Харта
        float temperature = (1.0 / (log(tr / THERMISTOR_R) / B + 1.0 / (NOMINAL_T + 273.15))) - 273.15;

        ESP.println("TEMP:" + String(temperature, 1));  // отправка на ESP
        Serial.println(temperature);

      } else {

        Serial.println("Неизвестная команда: " + cmd);
        
      }
    } else {
      
      Serial.println("От ESP: " + cmd);
      
    }
  }
}
