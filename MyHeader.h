#ifndef MY_HEADER_H //Il compilatore controlla se nella sua memoria esiste già una parola chiave (macro) che si chiama MY_HEADER_H
#define MY_HEADER_H //Se NO la crea, altrimenti va all'#endif

//Libraries for the functions
#include <Arduino.h>               // Fondamentale per usare String, millis(), pinMode ecc
#include <WiFi.h>
#include <UniversalTelegramBot.h>
#include <Ticker.h>
#include "SoilSensor.h"
#include "AirSensor.h"
#include <esp_task_wdt.h>


extern const char* SSID;
extern const char* PASSWORD;
extern const String CHAT_ID;

extern UniversalTelegramBot bot;
extern Ticker timerPompa;

extern bool watering;
extern bool end_watering;
extern unsigned long lastBotCheck;

extern const char* Startup_Menu_Telegram;

extern SoilSensor s1;
extern SoilSensor s2;
extern SoilSensor s3;
extern SoilSensor s4;
extern SoilSensor s5;
extern AirSensor clima;

extern const int RELE;
extern const int HALL_SENSOR_PIN;
extern const int LED_PIN;

//put your function headers here
void ConnectToWifi();

void AutoReconnect();

void StartWatering(int);

void StopWatering();

void HandleNewMessages();

void GetSoilMoistureSensorMeasurements(SoilSensor s);

void GetAirTempHumiSensorMeasurements();

void StopAndReset();

void UpdateStatusLED();

#endif