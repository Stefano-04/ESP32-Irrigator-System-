#ifndef MY_HEADER_H //Il compilatore controlla se nella sua memoria esiste già una parola chiave (macro) che si chiama MY_HEADER_H
#define MY_HEADER_H //Se NO la crea, altrimenti va all'#endif

//Libraries for the functions
#include <Arduino.h>               // Fondamentale per usare String, millis(), pinMode ecc
#include <WiFi.h>                  // To handle Wifi connection
#include <WiFiClientSecure.h>     // To keep the Wifi connection secure (ensure criptografic communication)
#include <UniversalTelegramBot.h>  // to communicate with Telegram servers
#include <Ticker.h>     //for handling timing operations like turning on-off the water-pump or status LED
#include <DHT.h>        //for the data for DHT sensor
#include <esp_task_wdt.h>
#include "time.h"
#include "SoilSensor.h" //for sensor management and initialization
#include "AirSensor.h"

#define ENABLE_SMTP
#define ENABLE_DEBUG
#include <ReadyMail.h> //for sending daily emails

extern volatile int contatoreImpulsiVento;
extern const char* SSID;
extern const char* PASSWORD;
extern const char* CHAT_ID;
extern const char* AUTHOR_EMAIL;
extern const char* AUTHOR_APP_PASS;
extern const char* AUTHOR_NAME;

extern const char* ThingSpeak_serverName;
extern const char* ThingSpeak_apiKey;

extern UniversalTelegramBot bot;
extern Ticker timerPompa;
extern SMTPClient smtp;

extern bool watering;
extern bool end_watering;
extern unsigned long lastBotCheck;

extern SoilSensor sensor1;
extern SoilSensor sensor2;
extern SoilSensor sensor3;
extern SoilSensor sensor4;
extern SoilSensor sensor5;
extern AirSensor sensor_dht;

extern const int RELAY_1;
extern const int RELAY_2;
extern const int HALL_SENSOR;
extern const int RAIN_SENSOR;
extern const int VCC_RAIN_SENSOR;
extern const int TEMPERATURE_HUMIDITY_SENSOR;
extern const int LED_BUILTIN;

extern const char* SMTP_HOST;
extern const int SMTP_PORT;

//Recipient's email
extern const char* RECIPIENT_EMAIL;
extern const char* RECIPIENT_NAME;

// Definiamo il tempo di timeout (es. 30 secondi)
// Deve essere superiore al tempo massimo che il tuo loop impiega per girare
#define WDT_TIMEOUT 30
#define MAX_WATERING_TIME 60  //Upper bound time (in seconds) for watering plants

//put your function headers here
void ContaImpulsi();//ISR

void ConnectToWifi();

void AutoReconnect();

void StartWatering(int);

void StopWatering();

void HandleNewMessages();

void GetSoilMoistureSensorMeasurements(SoilSensor sensor);

void GetAirTempHumiSensorMeasurements();

void StopAndReset();

void UpdateStatusLED();

void SendEmail();

void CheckAndSendDailyEmail();

void InviaDatiThingSpeak();

void CalcolaVentoOgniMinuto();

#endif