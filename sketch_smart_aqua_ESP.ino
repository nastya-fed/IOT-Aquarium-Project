#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

const char* ssid = "";
const char* password = "";
const char* BOT_TOKEN = "";

WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 3 * 3600, 60000);

unsigned long lastCheckTime = 0; //когда посл раз опрашивали тг

int scheduledHour = -1, scheduledMinute = -1; //время вкл света
bool triggeredToday = false; //свет вкл уже сегодня

int scheduledOffHour = -1, scheduledOffMinute = -1; //время выкл света
bool offTriggeredToday = false; //свет выкл уже сегодня

String lastChatId = ""; //посл чат для автоотправлений

//////
struct FeedTime {
  int hour;
  int minute;
};

FeedTime scheduledFeedTimes[5][2];  // мсв время кормления
int numScheduledFeedTimes = 0;      // кол-во введенных времен кормления
//////

bool autoTempEnabled = false; //отправляем ли темп автоматически
unsigned long lastTempSentTime = 0; //когда отправили посл раз темп

void setup() {
  Serial.begin(9600);
  delay(500);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1000);
  secured_client.setInsecure();
  timeClient.begin();
}

String readFromArduino() {

  String input = "";
  unsigned long startTime = millis();
  while (millis() - startTime < 1000) {
    while (Serial.available()) {

      char c = Serial.read();
      if (c == '\n') {

        input.trim();
        return input;

      }
      input += c;

    }
    delay(10);
  }
  return "";

}

void loop() {

  timeClient.update();

  if (millis() - lastCheckTime > 2000) {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

    while (numNewMessages) {

      for (int i = 0; i < numNewMessages; i++) {

        String text = bot.messages[i].text;
        String chat_id = bot.messages[i].chat_id;
        lastChatId = chat_id;

        if (text == "/lighton") {
          bot.sendMessage(chat_id, "Свет включен", "");
          Serial.println("CMD:ON");

        } else if (text == "/lightoff") {
          bot.sendMessage(chat_id, "Свет выключен", "");
          Serial.println("CMD:OFF");

        } else if (text.startsWith("/setlighton")) {//установка времени автовкл света

          String timeStr = text.substring(12);
          int sep = timeStr.indexOf(':');
          if (sep != -1) {

            int hour = timeStr.substring(0, sep).toInt();
            int minute = timeStr.substring(sep + 1).toInt();
            if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {

              scheduledHour = hour;
              scheduledMinute = minute;
              triggeredToday = false;
              bot.sendMessage(chat_id, "Время включения: " + timeStr, "");
            
            } else {
              bot.sendMessage(chat_id, "Неверный формат времени (0–23:0–59)", "");
            }
          } else {
            bot.sendMessage(chat_id, "Формат команды: /set HH:MM", "");
          }

        } else if (text.startsWith("/setlightoff")) { //установка времени автовыкл света

          String timeStr = text.substring(13);
          int sep = timeStr.indexOf(':');
          if (sep != -1) {

            int hour = timeStr.substring(0, sep).toInt();
            int minute = timeStr.substring(sep + 1).toInt();
            if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {

              scheduledOffHour = hour;
              scheduledOffMinute = minute;
              offTriggeredToday = false;
              bot.sendMessage(chat_id, "Время выключения: " + timeStr, "");

            } else {
              bot.sendMessage(chat_id, "Неверный формат времени (0–23:0–59)", "");
            }
          } else {
            bot.sendMessage(chat_id, "Формат команды: /setoff HH:MM", "");
          }

        } else if (text.startsWith("/setfood ")) { //установка времени автокормления

          String feedTimesStr = text.substring(9); 
          numScheduledFeedTimes = 0;             

          int startIndex = 0;
          while (startIndex < feedTimesStr.length() && numScheduledFeedTimes < 5) {

            int endIndex = feedTimesStr.indexOf(' ', startIndex);
            if (endIndex == -1) endIndex = feedTimesStr.length();

            String timeStr = feedTimesStr.substring(startIndex, endIndex);
            int sep = timeStr.indexOf(':');

            if (sep != -1) {

              int hour = timeStr.substring(0, sep).toInt();
              int minute = timeStr.substring(sep + 1).toInt();
              
              if (hour >= 0 && hour < 24 && minute >= 0 && minute < 60) {

                scheduledFeedTimes[numScheduledFeedTimes][0].hour = hour;
                scheduledFeedTimes[numScheduledFeedTimes][0].minute = minute;
                scheduledFeedTimes[numScheduledFeedTimes][1].hour = 0; 
                numScheduledFeedTimes++;

              }

            }

            startIndex = endIndex + 1;

          }

          bot.sendMessage(chat_id, "Время кормления установлено", "");

        } else if (text == "/feed") { //кормление по команде

          bot.sendMessage(chat_id, "Рыбки кушают! 🐠", "");
          Serial.println("CMD:FEED");

        } else if (text == "/temp") { //измерям температуру

          Serial.println("CMD:TEMP");  
          delay(500);                
          String tempResponse = readFromArduino();
          if (tempResponse.startsWith("TEMP:")) {

            bot.sendMessage(chat_id, "Температура воды: " + tempResponse.substring(5) + " °C", "");

          } else {
            bot.sendMessage(chat_id, "Ошибка: не удалось получить температуру", "");
          }

        } else if (text == "/autotemp") {

          autoTempEnabled = true;
          bot.sendMessage(chat_id, "Автоматическая передача температуры включена", "");

        } else if (text == "/stoptemp") {

          autoTempEnabled = false;
          bot.sendMessage(chat_id, "Автоматическая передача температуры остановлена", "");

        } else {

          bot.sendMessage(chat_id,
                          "Привет! Я бот для управления твоим умным аквариумом\n\n"
                          "    Температура:\n"
                          "    /temp - Получить текущую температуру\n"
                          "    /autotemp - Начать отправку температуры каждую секунду\n"
                          "    /stoptemp - Остановить отправку температуры\n\n"
                          "    Кормление:\n"
                          "    /feed - Покормить рыбок вручную\n"
                          "    /setfood HH:MM HH:MM HH:MM - Установка времени кормления\n\n"
                          "    Свет:\n"
                          "    /lighton - Включить свет\n"
                          "    /lightoff - Выключить свет\n"
                          "    /setlighton HH:MM - Включение света по расписанию\n"
                          "    /setlightoff HH:MM - Выключение света по расписанию",
                          "");

        }
      }
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastCheckTime = millis();

  }

  // блок автоуправления
  int currentHour = timeClient.getHours();
  int currentMinute = timeClient.getMinutes();
 

  if (scheduledHour == currentHour && scheduledMinute == currentMinute && !triggeredToday) {

    Serial.println("CMD:ON");
    triggeredToday = true;
    if (lastChatId != "") bot.sendMessage(lastChatId, "Автоуправление: свет включен", "");

  }
 
  if (scheduledOffHour == currentHour && scheduledOffMinute == currentMinute && !offTriggeredToday) {

    Serial.println("CMD:OFF");
    offTriggeredToday = true;
    if (lastChatId != "") bot.sendMessage(lastChatId, "Автоуправление: свет выключен", "");

  }

  // автокормление
  for (int i = 0; i < numScheduledFeedTimes; i++) {

    int h = scheduledFeedTimes[i][0].hour;
    int m = scheduledFeedTimes[i][0].minute;
    int fed = scheduledFeedTimes[i][1].hour;

    if (h == currentHour && m == currentMinute && fed == 0) {

      Serial.println("CMD:FEED");
      Serial.println("feed");
      scheduledFeedTimes[i][1].hour = 1;  // покормили в это время и отметили это
      if (lastChatId != "") bot.sendMessage(lastChatId, "Автокормление: рыбки кушают!", "");
      break;

    }

  }

  // автоотправление температуры каждую сек
  if (autoTempEnabled && millis() - lastTempSentTime >= 1000) {

    Serial.println("CMD:TEMP");
    delay(100);
    String tempResponse = readFromArduino();

    if (tempResponse.startsWith("TEMP:") && lastChatId != "") {

      bot.sendMessage(lastChatId, "Температура воды: " + tempResponse.substring(5) + " °C", "");

    }

    lastTempSentTime = millis();

  }

  // сброс флагов света и кормления
  if (currentHour == 0 && currentMinute == 0) {

    triggeredToday = false;
    offTriggeredToday = false;

    for (int i = 0; i < numScheduledFeedTimes; i++) {
      scheduledFeedTimes[i][1].hour = 0;
    }

  }

  delay(1000);
}
