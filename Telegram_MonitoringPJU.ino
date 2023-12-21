#define debugMode 1 //debugMode 1 enable common print serial, 0 only print error
#if debugMode == 1
#define debugP(x) Serial.println(x) //printSerial
#define debugE(y) Serial.println(y) //printError
#else
#define debugP(x)
#define debugE(y) Serial.println(y)
#endif

#define USE_CLIENTSSL true 
#define MYTZ "CET-1CEST,M3.5.0,M10.5.0/3"

#include <time.h>
#include <AsyncTelegram2.h>
#include <Adafruit_ADS1X15.h>
#include <WiFiManager.h>

#ifdef ESP8266
  #include <ESP8266WiFi.h>
  BearSSL::WiFiClientSecure client;
  BearSSL::Session   session;
  BearSSL::X509List  certificate(telegram_cert);
  
#elif defined(ESP32)
  #include <WiFi.h>
  #include <WiFiClient.h>
  #if USE_CLIENTSSL
    #include <SSLClient.h>  
    #include "tg_certificate.h"
    WiFiClient base_client;
    SSLClient client(base_client, TAs, (size_t)TAs_NUM, A0, 1, SSLClient::SSL_ERROR);
  #else
    #include <WiFiClientSecure.h>
    WiFiClientSecure client;  
  #endif
#endif

AsyncTelegram2 teleBot(client);
Adafruit_ADS1115 ads;

const char* APssid  =  "Monitoring PJU";     // SSID WiFi network
const char* APpass  =  "12345678";     // Password  WiFi network
const char* token =  "6797807079:AAEVc81RUZ9uKbleaj3WwoCnBihD4yOAqjM";  // Telegram token

byte lamp1Pin = 12; //D6
byte lamp2Pin = 13; //D7

// byte ldr1Pin = 12; //D6
// byte ldr2Pin = 13; //D7

bool lamp1Stat = true;
bool lamp2Stat = true;
String lamp1Condition = "Baik";
String lamp2Condition = "Baik";
String arus1;
String arus2;

void setup() {
  pinMode(lamp1Pin, OUTPUT);
  pinMode(lamp2Pin, OUTPUT);
  // pinMode(ldr1Pin, INPUT);
  // pinMode(ldr2Pin, INPUT);
  Serial.begin(115200);

  // WiFi.mode(WIFI_STA);
  // WiFi.begin(ssid, pass);
  // delay(500);
  // while (WiFi.status() != WL_CONNECTED) {
  //   Serial.print('.');
  //   delay(100);
  // }
  WiFiManager WM;
  WM.setConnectTimeout(10);
  if (WM.autoConnect(APssid, APpass)) {
    Serial.println("WiFi Terhubung");
  }
  else {
    Serial.println("Gagal Terhubung ke WiFi !");
  }

  Serial.println("Starting TelegramBot...");
#ifdef ESP8266
  // Sync time with NTP, to check properly Telegram certificate
  configTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  //Set certficate, session and some other base client properies
  client.setSession(&session);
  client.setTrustAnchors(&certificate);
  client.setBufferSizes(1024, 1024);
#elif defined(ESP32)
  // Sync time with NTP
  configTzTime(MYTZ, "time.google.com", "time.windows.com", "pool.ntp.org");
  #if USE_CLIENTSSL == false
    client.setCACert(telegram_cert);
  #endif
#endif
  
  // Set the Telegram bot properies
  teleBot.setUpdateTime(500);
  teleBot.setTelegramToken(token);

  // Check if all things are ok
  Serial.print("\nTest Telegram connection... ");
  teleBot.begin() ? Serial.println("OK") : Serial.println("NOK");

  if (!ads.begin()) {
    Serial.println("Failed to initialize ADS.");
    while (1);
  }
}

void loop() { 
  // local variable to store telegram message data
  TBMessage msg;
  incomingMsgHandling(msg);

  static uint32_t checkInterval = 0;
  if (millis() - checkInterval > 1000) {
    checkCondition(msg);
    checkInterval = millis();
  }
  
}

void incomingMsgHandling(TBMessage &msg) {
  if (teleBot.getNewMessage(msg)) {
    String msgText = msg.text;

    if (msgText.equals("/lampu1_on")) {                 // if the received message is "LIGHT ON"...
      lamp1Stat = 0;
      teleBot.sendMessage(msg, "Menyalakan Lampu 1");       // notify the sender
    }
    else if (msgText.equals("/lampu1_off")) {           // if the received message is "LIGHT OFF"...
      lamp1Stat = 1;
      teleBot.sendMessage(msg, "Mematikan Lampu 1"); 
    }
    else if (msgText.equals("/lampu2_on")) {
      lamp2Stat = 0;
      teleBot.sendMessage(msg, "Menyalakan Lampu 2");
    }
    else if (msgText.equals("/lampu2_off")) {
      lamp2Stat = 1;
      teleBot.sendMessage(msg, "Mematikan Lampu 2");
    }
    else if (msgText.equals("/info")) {
      String slamp1Stat = "OFF";
      String slamp2Stat = "OFF";
      if (lamp1Stat == 0) slamp1Stat = "ON";
      if (lamp1Stat == 0) slamp2Stat = "ON";
      
      String reply;
      reply = "Informasi" ;
      reply += "\nLampu 1";
      reply += "\nStatus :";
      reply += slamp1Stat;
      reply += "\nKondisi:";
      reply += lamp1Condition;
      reply += "\nArus   :";
      reply += arus1;
      reply += "\n ";
      reply += "\nLampu 2";
      reply += "\nStatus :";
      reply += slamp2Stat;
      reply += "\nKondisi:";
      reply += lamp2Condition;
      reply += "\nArus   :";
      reply += arus2;
      teleBot.sendMessage(msg, reply);    
    }

    else {
      String reply;
      reply = "Selamat Datang " ;
      reply += msg.sender.username;
      reply += "\nDaftar perintah :";
      reply += "\n/lampu1_on";
      reply += "\n/lampu2_on";
      reply += "\n/lampu1_off";
      reply += "\n/lampu2_off";
      reply += "\n/info";
      teleBot.sendMessage(msg, reply);                    // and send it
    }

    digitalWrite(lamp1Pin, lamp1Stat);
    digitalWrite(lamp2Pin, lamp2Stat);
  }
}

void checkCondition(TBMessage &msg){
  bool ldr1Stat = 1;
  bool ldr2Stat = 1;
  int16_t lamp1Adc, lamp2Adc, ldr1Adc, ldr2Adc;
  lamp1Adc = ads.readADC_SingleEnded(0);
  lamp2Adc = ads.readADC_SingleEnded(1);
  ldr1Adc = ads.readADC_SingleEnded(2);
  ldr2Adc = ads.readADC_SingleEnded(3);
  debugP("Arus1 "+String(lamp1Adc));
  debugP("Arus2 "+String(lamp2Adc));
  debugP("LDR1  "+String(ldr1Adc));
  debugP("LDR2  "+String(ldr2Adc));

  if (ldr1Adc >= 100) ldr1Stat = 0;
  if (ldr2Adc >= 100) ldr2Stat = 0;

  if (lamp1Condition == "Baik" && lamp1Stat == 0 && lamp1Adc < 100 && ldr1Stat == 0){
    lamp1Condition = "Rusak";
    arus1 = "-";
    teleBot.sendMessage(msg, "Lampu 1 rusak !");
  }
  else if (lamp1Condition == "Rusak" && lamp1Adc > 100){
    lamp1Condition = "Baik";
    arus1 = "OK";
  }

  if (lamp2Condition == "Baik" && lamp2Stat == 0 && lamp2Adc < 100 && ldr2Stat == 0){
    lamp2Condition = "Rusak";
    arus2 = "-";
    teleBot.sendMessage(msg, "Lampu 2 rusak !");
  }
  else if (lamp2Condition == "Rusak" && lamp2Adc > 100){
    lamp2Condition = "Baik";
    arus2 = "OK";
  }
}
