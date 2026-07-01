#include <WiFi.h>                  // To handle Wifi connection
#include <WiFiClientSecure.h>      // To keep the Wifi connection secure (ensure criptografic communication)
#include <UniversalTelegramBot.h>  // to communicate with Telegram servers
#include <ArduinoJson.h>           //Handle Telegram messages, separating all the informations
#include <MyLoginExample.h>               // where WiFi credentials and Telegram bot token and chat_id are saved
#include <DHT.h>                   //for the data for DHT sensor
#include "SoilSensor.h"            //for sensor management and initialization
#include "AirSensor.h"  
#include "MyHeader.h"              //Contains project functions 
#include <Ticker.h>   //for handling timing operations like turning on-off the water-pump or status LED

Ticker timerPompa;    // Timer per la pompa
Ticker timerSensori;  // Timer per richedere nuovi dati ai sensori (es. ogni 30 min)

#include "esp_task_wdt.h"

// Definiamo il tempo di timeout (es. 30 secondi)
// Deve essere superiore al tempo massimo che il tuo loop impiega per girare
#define WDT_TIMEOUT 30

#define MAX_WATERING_TIME 60  //Upper bound time (in seconds) for watering plants

const int RELE=4;  // GPIO4 where is connected the relé that powers the water pump

const int LED_PIN=2;  //Built_in led


const int HALL_SENSOR_PIN=15; //Hall Sensor PIN for the anemometer

#define N 5  //numero letture per dato

AirSensor clima(33, DHT11);

//Import of MyLogin.h secret passwords
const char* SSID = MY_SSID1;
const char* PASSWORD = MY_PASSWORD1;

const String BOT_TOKEN = BOT_TOKEN_IRRIGATOR;
const String CHAT_ID = CHAT_ID_BOT_IRRIGATOR;

WiFiClientSecure client;                      //define the client for the Wifi Connection
UniversalTelegramBot bot(BOT_TOKEN, client);  //define the bot


// These are the info displayed on Telegram chat when requested
const char* Startup_Menu_Telegram =
  "Commands:\n\n"
  "/start - restart the bot \n"
  "/stop - delete bot commands\n"
  "/water X - start watering for X seconds\n"
  "/realtime - display real-time measurements\n"
  "/getdata - get instant data from Arduino sensors";

// These are important flags (inizialization)
bool watering = false;  //not watering
unsigned long wateringEnd = 0;
unsigned long lastBotCheck = 0;  // Tempo dell'ultimo controllo Telegram
bool pumpPower = false;          //pump is not powered
bool internet_down = false;      //internet is not down

bool end_watering = false;
bool end_readsensors = false;

  // Inizializzazione Sensori con pin analogici ESP32 per i sensori di umidità del terreno
  SoilSensor s1(36, "Vaso Gerani 1", 3000, 1700);
  SoilSensor s2(35, "Vaso Gerani 2", 3100, 1600);
  SoilSensor s3(34, "Vaso Gerani 3", 4095, 1500); //not available
  SoilSensor s4(39, "Vaso Gerani 4", 4095, 1500); 
  SoilSensor s5(32, "Vaso Gerani 5", 3200, 1700);




void setup() {
  //Establish a serial communication
  Serial.begin(115200);

  //DHT and Soil Moisture sensor SETUP
  clima.begin();
  s1.begin();
  s2.begin();
  s3.begin();
  s4.begin();
  s5.begin();

  // 1. Inizializza il Watchdog con il timeout e l'opzione di panico (reset)
  esp_task_wdt_config_t wdt_config = {
  .timeout_ms = WDT_TIMEOUT * 1000, 
  .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, 
  .trigger_panic = true 
};
esp_task_wdt_reconfigure(&wdt_config);
  // 2. Aggiunge il loop corrente al controllo del Watchdog
  esp_task_wdt_add(NULL);

  //SSL Certificate for Telegram
  client.setInsecure();
  client.setHandshakeTimeout(30000);  // 30 secondi di timeout per aspettare il server di Telegram (in caso di connessione lenta)

  //Establish a Wifi connection: ConnectToWifi()
  ConnectToWifi();

  timerSensori.attach(1800, []() {
    end_readsensors = true;
  });  //Imposta il timer per i dati ogni 30 minuti (1800 secondi)

  //PinModes for led, sensors and pumps
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELE, OUTPUT);
  digitalWrite(RELE, LOW);  //alimentazione pompa spenta

  pinMode(HALL_SENSOR_PIN, INPUT); // Set sensor pin as input
  bot.sendMessage(CHAT_ID, Startup_Menu_Telegram, "");
}

void loop() {
  // Resetta il timer del Watchdog: "Tutto ok, sono ancora vivo!"
  esp_task_wdt_reset();

  //0 Led status (for the connection) (not-blocking)
  UpdateStatusLED();

  //1 While connection is down: AutoReconnect()
  if (WiFi.status() != WL_CONNECTED) {
    AutoReconnect();
  }

  //Control new messages from Telegram HandleNewMessages() every 3 seconds
  //Read incoming messages (one at a time) and do a switch case construct, calling the appropraite functions)
  if (millis() - lastBotCheck > 3000) {
    HandleNewMessages();
    lastBotCheck = millis();
  }
}
