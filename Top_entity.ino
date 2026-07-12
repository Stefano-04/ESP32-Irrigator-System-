#include <MyLoginExample.h>               // where WiFi credentials and Telegram bot token and chat_id are saved       
#include "MyHeader.h"              //Contains project functions 

// Sender SMTP settings (GMAIL)
// Change if using a different provider
const char* SMTP_HOST="smtp.gmail.com";
const int SMTP_PORT=465;

//Recipient's email
const char* RECIPIENT_EMAIL="stefano.ghiotti10034@gmail.com";
const char* RECIPIENT_NAME="Stefano";

//ThingsSpeak API and Server Name
const char* ThingSpeak_serverName = MY_ThingSpeak_serverName;
const char* ThingSpeak_apiKey = MY_ThingSpeak_apiKey;

Ticker timerPompa;    // Timer per la pompa
Ticker timerSensori;  // Timer per richedere nuovi dati ai sensori (es. ogni 30 min), impostiamo il campionamento dei sensori e l'invio dati a Thingspeak ogni 30 min

//Import of MyLogin.h credentials
const char* SSID = MY_SSID1;
const char* PASSWORD = MY_PASSWORD1;
const char* BOT_TOKEN = BOT_TOKEN_IRRIGATOR;
const char* CHAT_ID = CHAT_ID_BOT_IRRIGATOR;
const char* AUTHOR_EMAIL=MY_AUTHOR_EMAIL;
const char* AUTHOR_APP_PASS=MY_AUTHOR_APP_PASS;
const char* AUTHOR_NAME=MY_AUTHOR_NAME;

WiFiClientSecure client;    //define the client for the Wifi Connection                
UniversalTelegramBot bot(BOT_TOKEN, client);  //define the bot
SMTPClient smtp(client);

/*FLAGS*/
bool watering = false;  //not watering
unsigned long wateringEnd = 0;
unsigned long lastBotCheck = 0;  // Tempo dell'ultimo controllo Telegram
//bool pumpPower = false;          //pump is not powered
bool internet_down = false;      //internet is not down

bool end_watering = false;
bool end_readsensors = false;

/*SENSORI*/
// Inizializzazione Sensori con pin analogici ESP32 per i sensori di umidità del terreno
SoilSensor sensor1(36, "Vaso Gerani 1", 3000, 1700);  //SVP
SoilSensor sensor2(35, "Vaso Gerani 2", 3100, 1600);  //P35
SoilSensor sensor3(34, "Vaso Gerani 3", 3100, 1500);  //P34
SoilSensor sensor4(39, "Vaso Gerani 4", 3100, 1500);  //SVN
SoilSensor sensor5(32, "Vaso Gerani 5", 3200, 1700);  //P32


const int HALL_SENSOR=25; //Hall Sensor PIN for the anemometer P25
const int RAIN_SENSOR=33;
const int VCC_RAIN_SENSOR=26;
const int TEMPERATURE_HUMIDITY_SENSOR=13;
AirSensor sensor_dht(TEMPERATURE_HUMIDITY_SENSOR, DHT11); //Pin per sensore dht pin 13 P13

const int RELAY_1=27;  //Pin per RELAY_1 P16
const int RELAY_2=14;  //Pin per RELAY_2 P17

const int LED_BUILTIN=2;

void setup() {
  //Establish a serial communication
  //Serial.begin(115200);
  //DHT and Soil Moisture sensor SETUP
  sensor_dht.begin();
  sensor1.begin();
  sensor2.begin();
  sensor3.begin();
  sensor4.begin();
  sensor5.begin();

//Inizializza il Watchdog con il timeout e l'opzione di panico (reset)
  esp_task_wdt_config_t wdt_config = {
  .timeout_ms = WDT_TIMEOUT * 1000, 
  .idle_core_mask = (1 << portNUM_PROCESSORS) - 1, 
  .trigger_panic = true 
  };

  esp_task_wdt_reconfigure(&wdt_config);
  // Aggiunge il loop corrente al controllo del Watchdog
  esp_task_wdt_add(NULL);

  //SSL Certificate for Telegram
  client.setInsecure();
  client.setHandshakeTimeout(30000);  // 30 secondi di timeout per aspettare il server di Telegram (in caso di connessione lenta)

  //Establish a Wifi connection: ConnectToWifi()
  ConnectToWifi();

  // Impostazioni orario per l'Italia (GMT+1 e 1 ora di ora legale) per i timer (time.h)
  // Set NTP config time
  const long gmtOffset_sec = 3600; 
  const int daylightOffset_sec = 3600; 
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");


  timerSensori.attach(1800, []() {//Imposta il timer per i dati ogni 30 minuti (1800 secondi)
    end_readsensors = true;
  });  

  //PinModes for led, sensors and relays
  pinMode(HALL_SENSOR, INPUT_PULLUP);  // Set sensor pin as input
  // Collega l'interrupt: 
  // 1. Quale pin? (usiamo digitalPinToInterrupt per sicurezza)
  // 2. Quale funzione chiamare? (ContaImpulsi)
  // 3. Quando? FALLING significa "quando il segnale passa da HIGH a LOW" (il magnete è arrivato)
  attachInterrupt(digitalPinToInterrupt(HALL_SENSOR), ContaImpulsi, FALLING);
  
  pinMode(LED_BUILTIN, OUTPUT);     //Set LED_BUILTIN for intermittent Blinking (check for Internet connection) pin2 GPIO2
  pinMode(RELAY_1, OUTPUT);            //Set RELAY_1 pin as output
  digitalWrite(RELAY_1, LOW);  //alimentazione pompa spenta (low->pompa spenta)
  pinMode(RELAY_2, OUTPUT);            //Set RELAY_2 pin as output
  digitalWrite(RELAY_2, LOW);  //alimentazione pompa spenta (low->pompa spenta)
  pinMode(RAIN_SENSOR, INPUT);
  pinMode(VCC_RAIN_SENSOR, OUTPUT);
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

  CalcolaVentoOgniMinuto();//ogni 60sec contiamo il numero di impulsi del sensore hall per stimare la velocità del vento.

  if (end_readsensors == true) { //quando scatta il timer dei 30 minuti leggo i sensori e invio i dati a ThingSpeak
    InviaDatiThingSpeak(); // Chiamiamo la nuova funzione!
    end_readsensors = false; // Riazzera l'allarme
  }
  //Control new messages from Telegram HandleNewMessages() every 3 seconds
  //Read incoming messages (one at a time) and do a switch case construct, calling the appropraite functions)
  if (millis() - lastBotCheck > 3000) {
    HandleNewMessages();
    lastBotCheck = millis();
  }
}