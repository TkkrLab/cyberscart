/*
 * CyberSCART: the IoT SCART switch
 * Copyright 2018 Renze Nicolai
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all copies or
 * substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <ESP8266WiFi.h>
#include "FS.h"
#include <PubSubClient.h>
#include <Wire.h>

#define PIN_LED D0
#define PIN_SELECT_1 D7
#define PIN_SELECT_2 D8

#define PIN_FP_1 D2
#define PIN_FP_2 D3
#define PIN_FP_3 D4
#define PIN_FP_4 D5
#define PIN_FP_B D6

WiFiClient espClient;
PubSubClient client(espClient);

#define SSID_LENGTH 64 + 1
#define PASSWORD_LENGTH 64 + 1
#define SERVER_LENGTH 64 + 1
#define NAME_LENGTH 64 + 1
#define CHANNEL_LENGTH 64 + 1

struct struct_settings {
  char ssid[SSID_LENGTH];
  char password[PASSWORD_LENGTH];
  char server[SERVER_LENGTH];
  char name[NAME_LENGTH];
  char channel[CHANNEL_LENGTH];
  char mqtt_username[NAME_LENGTH];
  char mqtt_password[NAME_LENGTH];
} settings;

uint8_t currentChannel = 0;

const int struct_settings_size = sizeof(struct_settings);

bool loadSettings() {
  File file = SPIFFS.open("/settings", "r");
  if (!file) {
    Serial.println(F("Settings file read error!"));
    return false;
  }
  size_t file_size = file.size();
  if (file_size!=struct_settings_size) {
    Serial.println(F("Settings file size mismatch!"));
    Serial.println(file_size);
    Serial.println(struct_settings_size);
    //return false;
  }
  uint8_t* settings_ptr = (uint8_t*) &settings;
  for(uint32_t i = 0; i<file_size; i++){
    settings_ptr[i] = file.read();
  }
  file.close();
  return true;
}

void saveSettings() {
  File file = SPIFFS.open("/settings", "w");
  if (!file) {
    Serial.println(F("Settings file write error!"));
    return;
  }
  uint8_t* settings_ptr = (uint8_t*) &settings;
  for(uint32_t i = 0; i<struct_settings_size; i++){
    file.write(settings_ptr[i]);
  }
  file.close();
}

void printSettings() {
  Serial.println(F("Configuration"));
  Serial.println(F("================================="));
  Serial.print(F("SSID: "));
  Serial.println(settings.ssid);
  //Serial.print(F("PASSWORD: "));
  //Serial.println(settings.password);
  Serial.print(F("SERVER: "));
  Serial.println(settings.server);
  Serial.print(F("DEVICE NAME: "));
  Serial.println(settings.name);
  Serial.print(F("CHANNEL: "));
  Serial.println(settings.channel);
  Serial.print(F("USERNAME: "));
  Serial.println(settings.mqtt_username);
  //Serial.print(F("PASSWORD: "));
  //Serial.println(settings.mqtt_password);
  Serial.println(F("================================="));
}

void serialInput(char* buffer, uint16_t len) {
  while(Serial.available()) Serial.read();
  for (uint16_t i = 0; i<len; i++) buffer[i] = 0;
  uint16_t pos = 0;
  while(pos<len) {
    while(Serial.available()) {
      buffer[pos] = Serial.read();
      if ((buffer[pos]=='\n') || (buffer[pos]=='\r')) {
        buffer[pos] = 0;
        Serial.println();
        while(Serial.available()) Serial.read();
        return;
      } else {
        Serial.print(buffer[pos]);
      }
      pos++;
    }
  }
}

void configWizard() {
  while(Serial.available()) Serial.read(); //Flush
  Serial.println(F("=== SETUP ==="));
  Serial.println(F("SSID: "));
  serialInput(settings.ssid, SSID_LENGTH);
  Serial.print(F("SSID set to: "));
  Serial.println(settings.ssid);
  Serial.println(F("PASSWORD: "));
  serialInput(settings.password, PASSWORD_LENGTH);
  Serial.print(F("PASSWORD set to: "));
  Serial.println(settings.password);
  Serial.println(F("SERVER: "));
  serialInput(settings.server, SERVER_LENGTH);
  Serial.print(F("SERVER set to: "));
  Serial.println(settings.server);
  Serial.println(F("DEVICE NAME: "));
  serialInput(settings.name, NAME_LENGTH);
  Serial.print(F("DEVICE NAME set to: "));
  Serial.println(settings.name);
  Serial.println(F("CHANNEL: "));
  serialInput(settings.channel, CHANNEL_LENGTH);
  Serial.print(F("CHANNEL set to: "));
  Serial.println(settings.channel);
  Serial.println(F("MQTT USER: "));
  serialInput(settings.mqtt_username, NAME_LENGTH);
  Serial.print(F("MQTT USERNAME set to: "));
  Serial.println(settings.mqtt_username);
  Serial.println(F("MQTT PASSWORD: "));
  serialInput(settings.mqtt_password, NAME_LENGTH);
  Serial.print(F("MQTT PASSWORD set to: "));
  Serial.println(settings.mqtt_password);
  Serial.println("Done!");
  saveSettings();
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_SELECT_1, OUTPUT);
  pinMode(PIN_SELECT_2, OUTPUT);
  pinMode(PIN_FP_1, OUTPUT);
  pinMode(PIN_FP_2, OUTPUT);
  pinMode(PIN_FP_3, OUTPUT);
  pinMode(PIN_FP_4, OUTPUT);
  Serial.begin(115200);
  Serial.println(F("ESP8266 composite video switcher - Renze Nicolai 2018"));
  if (!SPIFFS.begin()) {
    Serial.println("Failed to mount file system! HALT");
    while (1) yield();
  }
  if (!loadSettings()) {
    configWizard();
    printSettings();
    if (!loadSettings()) {
      Serial.println(F("Fatal error. HALT"));
      while (1) yield();
    }
  }
  printSettings();
  WiFi.mode(WIFI_STA);
  WiFi.begin(settings.ssid, settings.password);
  WiFi.hostname(settings.name);
  
  Serial.println(F("press [s] for setup..."));

  client.setServer(settings.server, 1883);
  client.setCallback(callback);

  applyChannel();
}

void setInternalLed(bool state) {
  digitalWrite(PIN_LED, state);
}

void applyChannel() {
  Serial.print("Switching to channel ");
  Serial.print(currentChannel);
  Serial.println(".");

  digitalWrite(PIN_FP_1, LOW);
  digitalWrite(PIN_FP_2, LOW);
  digitalWrite(PIN_FP_3, LOW);
  digitalWrite(PIN_FP_4, LOW);

  if (currentChannel == 0) digitalWrite(PIN_FP_1, HIGH);
  if (currentChannel == 1) digitalWrite(PIN_FP_2, HIGH);
  if (currentChannel == 2) digitalWrite(PIN_FP_3, HIGH);
  if (currentChannel == 3) digitalWrite(PIN_FP_4, HIGH);
  
  digitalWrite(PIN_SELECT_1, !(currentChannel & 0x01));
  digitalWrite(PIN_SELECT_2, !((currentChannel>>1) & 0x01));
}

void callback(char* topic, byte* payload, unsigned int length) {
  String endpoint = topic;
  endpoint.remove(0,strlen(settings.channel));

  char buffer[3];
  buffer[0] = payload[0];
  buffer[1] = 0;

  uint8_t ch = atoi(buffer);

  if (endpoint=="/channel") {
    currentChannel = ch;
    applyChannel();
  } else {
    Serial.print("Ignored unknown endpoint: ");
    Serial.println(endpoint);
  }
}

bool previousButton = false;

void inputHandle() {
  bool button = digitalRead(PIN_FP_B);
  if (button!=previousButton) { 
    previousButton = button;
    if (button) {
      currentChannel++;
      if (currentChannel>3) currentChannel = 0;
      applyChannel();
      client.publish(String(String(settings.channel)+"/channel").c_str(), String(currentChannel).c_str());
      delay(500);
    }
  }
}

unsigned long previousMillis = 0;
const unsigned long interval = 500; //Interval in milliseconds

void loop() {
  inputHandle();
  
  while (Serial.available()) {
    char key = Serial.read();
    if (key=='s') configWizard();
  }
  if ((!client.connected())&&(WiFi.status() == WL_CONNECTED)) {
    Serial.println(F("Wifi connected! Connecting to MQTT server..."));
    Serial.println("<"+String(settings.server)+">");
    if (client.connect(settings.name, settings.mqtt_username, settings.mqtt_password)) {
      Serial.println("MQTT connected!");
      setInternalLed(true);
      //client.publish("outTopic", "hello world");
      client.subscribe(String(String(settings.channel)+"/#").c_str());
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    setInternalLed(false);
    Serial.println(F("Waiting for wifi..."));
    for (uint8_t i = 0; i<100; i++) {
      inputHandle();
      delay(10);
    }
  }
  client.loop();
  unsigned long currentMillis = millis();
  if(currentMillis - previousMillis > interval) {
    previousMillis = currentMillis;
    inputHandle();
    //Serial.println("Loop");
  }
}
