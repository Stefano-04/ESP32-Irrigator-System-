#include <WiFi.h>                  // To handle Wifi connection
#include <WiFiClientSecure.h>      // To keep the Wifi connection secure (ensure criptografic communication)
#include <UniversalTelegramBot.h>  // to communicate with Telegram servers
#include <ArduinoJson.h>           //Handle Telegram messages, separating all the informations
#include <MyLogin.h>               // where WiFi credentials and Telegram bot token and chat_id are saved
#include <DHT.h>                   //for the data for DHT sensor

#include <Ticker.h>   //for handling timing operations like turning on-off the water-pump or status LED
Ticker timerPompa;    // Timer per la pompa
Ticker timerSensori;  // Timer per richedere nuovi dati ai sensori (es. ogni 30 min)

#include "esp_task_wdt.h"

// Definiamo il tempo di timeout (es. 30 secondi)
// Deve essere superiore al tempo massimo che il tuo loop impiega per girare
#define WDT_TIMEOUT 30

#define MAX_WATERING_TIME 60  //Upper bound time (in seconds) for watering plants

#define RELE 4  // GPIO4 where is connected the relé that powers the water pump

#define LED_PIN 2  //Built_in led

#define N 5  //numero letture per dato

#define DHTPIN 13          // Pin a cui è collegato il DHT (esempio GPIO 13) DA CAMBIARE
#define DHTTYPE DHT22      // O DHT11 DA CAMBIARE
DHT dht(DHTPIN, DHTTYPE);  //Define dht object

//Pin analogici ESP32 per i sensori di umidità del terreno
#define S1 36
#define S2 35
#define S3 34
#define S4 33
#define S5 32

//Import of MyLogin.h secret passwords
const char* SSID = MY_SSID1;
const char* PASSWORD = MY_PASSWORD1;

const String BOT_TOKEN = BOT_TOKEN_IRRIGATOR;
const String CHAT_ID = CHAT_ID_BOT_IRRIGATOR;

WiFiClientSecure client;                      //define the client for the Wifi Connection
UniversalTelegramBot bot(BOT_TOKEN, client);  //define the bot


// These are the info displayed on Telegram chat when requested
const char* info =
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


struct Sensor {
  int pin;          // Il GPIO a cui è collegato
  String nome;      // Il nome da visualizzare su Telegram (es. "Terreno Balcone")
  float media;      // L'ultimo valore medio calcolato
  int valoreAria;   // Valore calibrazione secco (opzionale, utile per sensori terreno)
  int valoreAcqua;  // Valore calibrazione bagnato (opzionale)

  // Funzione interna all'oggetto per leggere il valore analogico
  void leggi() {
    long somma = 0;
    for (int i = 0; i < N; i++) {
      somma += analogRead(pin);
      delay(200);
    }
    media = (float)somma / N;
  }
};

Sensor s1, s2, s3, s4, s5;

void ConnectToWifi() {
  int tentativi = 0;
  WiFi.begin(SSID, PASSWORD);
  Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED && tentativi < 20) {
    Serial.print(".");
    esp_task_wdt_reset();
    delay(500);
    tentativi++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ WiFi connected");
    bot.sendMessage(CHAT_ID, "✅ WiFi connected", "");
  } else {
    // Se fallisce, lo diciamo sulla seriale ma non blocchiamo il programma
    Serial.println("\n❌ Connessione fallita al primo colpo");
  }
}

void AutoReconnect() {
  // Usiamo static per memorizzare il tempo tra una chiamata e l'altra
  static unsigned long ultimoTentativo = 0;

  // Riprova solo una volta ogni 10 secondi per non sovraccaricare il processore
  if (millis() - ultimoTentativo > 10000) {
    Serial.println("Tentativo di riconnessione WiFi...");
    WiFi.begin(SSID, PASSWORD);
    ultimoTentativo = millis();
  }
}

void StartWatering(int seconds) {
  if (seconds <= 0 || seconds > 60) {
    seconds = 10;  // Sicurezza
  }
  digitalWrite(RELE, HIGH);
  watering = true;
  timerPompa.once(seconds, StopWatering);  // Spegnimento garantito
}

void StopWatering() {
  digitalWrite(RELE, LOW);
  watering = false;
  end_watering = true;
  if (end_watering) {
    bot.sendMessage(CHAT_ID, "Irrigazione completata con successo!", "");
    end_watering = false;
  }
}

void HandleNewMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);

  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      String text = bot.messages[i].text;
      String from_id = String(bot.messages[i].chat_id);

      if (from_id != CHAT_ID) {
        bot.sendMessage(from_id, "Utente non autorizzato.", "");
        continue;
      }

      if (text == "/start") {
        bot.sendMessage(CHAT_ID, info, "");
      } else if (text.startsWith("/water")) {
        int spazio = text.indexOf(" ");
        int durata = (spazio != -1) ? text.substring(spazio + 1).toInt() : 10;
        StartWatering(durata);
        bot.sendMessage(CHAT_ID, "Pompa accesa per " + String(durata) + " secondi.", "");
      } else if (text == "/getdata") {
        // Avviamo la lettura
        GetSoilMoistureSensorMeasurements(s1);
        GetSoilMoistureSensorMeasurements(s2);
        GetSoilMoistureSensorMeasurements(s3);
        GetSoilMoistureSensorMeasurements(s4);
        GetSoilMoistureSensorMeasurements(s5);
      } else if (text == "/realtime") {
        // Avviamo la lettura
        GetAirTempHumiSensorMeasurements();
      } else if (text == "/stop") {
        StopWatering();
        StopAndReset();
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void GetSoilMoistureSensorMeasurements(Sensor& s) {
  // Pick 5 measurements and do the average
  s.leggi();  // L'oggetto legge se stesso e aggiorna la sua proprietà .media
  // Calcolo della percentuale usando i parametri dell'oggetto
  Serial.println(s.nome+". "+s.media);
  int percentuale = map(s.media, s.valoreAria, s.valoreAcqua, 0, 100);
 // percentuale = constrain(percentuale, 0, 100);

  String messaggio = "📊 " + s.nome + ": " + String(percentuale) + "%";
  bot.sendMessage(CHAT_ID, messaggio, "");
  //Serial.println(messaggio);
}

void GetAirTempHumiSensorMeasurements() {
  // Pick 5 measurements and do the average
}

void StopAndReset() {  //Interrupt what ESP32 is doing and return in "IDLE" state: red button
  Serial.println("🚨 RESET DI EMERGENZA AVVIATO...");

  // 1. FERMA L'HARDWARE
  digitalWrite(RELE, LOW);  // Spegne subito la pompa
  watering = false;         // Reset flag irrigazione

  // 2. FERMA I TIMER (Ticker)
  timerPompa.detach();  // Impedisce che il Ticker provi a chiamare StopWatering in futuro

  // 3. SVUOTA LA CODA TELEGRAM
  // Chiediamo a Telegram tutti i messaggi arretrati e NON facciamo nulla.
  // Questo aggiorna l'offset sul server, "bruciando" i comandi vecchi.
  int numArretrati = bot.getUpdates(bot.last_message_received + 1);
  while (numArretrati > 0) {
    bot.last_message_received = bot.messages[numArretrati - 1].update_id;
    numArretrati = bot.getUpdates(bot.last_message_received + 1);
  }
  lastBotCheck = millis();  // Evita un controllo immediato

  bot.sendMessage(CHAT_ID, "🚨 Sistema resettato. Pompa spenta e comandi pendenti cancellati.", "");

  // OPZIONALE: Se vuoi un vero "ripartire da zero" fisico
  // Serial.println("Riavvio fisico dell'ESP32...");
  // ESP.restart();
}

//Feedback LED: Usa un LED (anche quello integrato nel pin 2) per segnalare lo stato.
//Luce fissa = Connesso, Lampeggio veloce = Errore WiFi, Respiro (fading) = Irrigazione in corso.
void UpdateStatusLED() {
  if (WiFi.status() != WL_CONNECTED) {
    // Lampeggio veloce usando millis() per non bloccare il codice
    digitalWrite(LED_PIN, (millis() / 100) % 2);
  } else if (watering) {
    digitalWrite(LED_PIN, (millis() / 500) % 2);  // Lampeggio medio durante irrigazione
  } else {
    digitalWrite(LED_PIN, HIGH);  // Fisso se tutto ok
  }
}

void setup() {
  //Establish a serial communication
  Serial.begin(115200);
  //DHT SETUP
  dht.begin();

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


  //setCpuFrequencyMhz(80);   // Reduce  heat dissipation by adjusting the clock frequency to 80MHz (from 240)
  //WiFi.setSleep(true);    // Risparmio energetico WiFi

  // Inizializzazione Sensori
  s1 = { S1, "Vaso Gerani 1", 0.0, 3000, 1700 };
  s2 = { S2, "Vaso Gerani 2", 0.0, 3100, 1600};
  s3 = { S3, "Vaso Gerani 3", 0.0, 4095, 1500 }; //error
  s4 = { S4, "Vaso Gerani 4", 0.0, 4095, 1500 }; //error
  s5 = { S5, "Vaso Gerani 5", 0.0, 3200, 1700 };

  pinMode(s1.pin, INPUT);
  pinMode(s2.pin, INPUT);
  pinMode(s3.pin, INPUT);
  pinMode(s4.pin, INPUT);
  pinMode(s5.pin, INPUT);
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

  //2 Control new messages from Telegram HandleNewMessages() every 3 seconds
  //Read incoming messages (one at a time) and do a switch case construct, calling the appropraite functions)
  if (millis() - lastBotCheck > 3000) {
    HandleNewMessages();
    lastBotCheck = millis();
  }
}
