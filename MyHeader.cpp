#include "MyHeader.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>           //Handle Telegram messages, separating all the informations

const float TEMP_MAX=60.0;
const float TEMP_MIN=-30.0;
int i=0;//numero di misure già fatte nel corso della giornata (air_temp, air_humi) per il report giornaliero via mail
//Important infos displayed on Telegram chat when requested
const char* Startup_Menu_Telegram =
  "Commands:\n\n"
  "/start - restart the bot \n"
  "/stop - delete bot commands\n"
  "/water X - start watering for X seconds\n"
  "/realtime - display real-time measurements\n"
  "/getdata - get instant data from Arduino sensors";

  float air_temperature_somma=0.0; //vettore temperatura giornaliere
  float air_humidity_somma=0.0;    //vettore umidità giornaliere
  float air_temperature_max=TEMP_MIN; //temperatura massima giornaliera
  float air_temperature_min=TEMP_MAX; //temepratura minima giornaliera

  volatile int contatoreImpulsiVento = 0;
  unsigned long ultimoAzzeramentoVento = 0; // Timer per il minuto
  float ultimaVelocitaVentoCalcolata = 0.0; // Memorizza l'ultimo calcolo valido

void IRAM_ATTR ContaImpulsi() { //ISR per il conteggio del sensore Hall
  contatoreImpulsiVento++; // Aggiunge 1 ogni volta che il magnete passa
}
void ConnectToWifi() {
  int tentativi = 0;
  WiFi.begin(SSID, PASSWORD);
  //Serial.print("WiFi connecting");
  while (WiFi.status() != WL_CONNECTED && tentativi < 20) {
    Serial.print(".");
    esp_task_wdt_reset();
    delay(500);
    tentativi++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("\n✅ WiFi connected");
    bot.sendMessage(CHAT_ID, "✅ WiFi connected", "");
    bot.sendMessage(CHAT_ID, Startup_Menu_Telegram, "");
  } else {
    // Se fallisce, lo diciamo sulla seriale ma non blocchiamo il programma
    //Serial.println("\n❌ Connessione fallita al primo colpo");
  }
}

void AutoReconnect() {
  // Usiamo static per memorizzare il tempo tra una chiamata e l'altra
  static unsigned long ultimoTentativo = 0;

  // Riprova solo una volta ogni 10 secondi per non sovraccaricare il processore
  if (millis() - ultimoTentativo > 10000) {
    //Serial.println("Tentativo di riconnessione WiFi...");
    WiFi.begin(SSID, PASSWORD);
    ultimoTentativo = millis();
  }
}

void StartWatering(int seconds) {
  if (seconds <= 0 || seconds > 60) {
    seconds = 10; //Imposto un valore basso in caso in cui ci sia un errore
  }
  digitalWrite(RELAY_1, HIGH); //Avvia l'irrigazione (HIGH->POMPA FUNZIONANTE)
  digitalWrite(RELAY_2, HIGH);
  watering = true;
  timerPompa.once(seconds, StopWatering);  // Setup per lo Spegnimento
}

void StopWatering() {
  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);
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
      const char* c_text = bot.messages[i].text.c_str();
      String from_id = String(bot.messages[i].chat_id);

      if (from_id != CHAT_ID) {
        bot.sendMessage(from_id, "Utente non autorizzato.", "");
        continue;
      }

      if (strcmp(c_text, "/start") == 0) {
        bot.sendMessage(CHAT_ID, Startup_Menu_Telegram, "");
      } 
      else if (strncmp(c_text, "/water", 6) == 0) {
        int durata = 10; 
        
        sscanf(c_text, "/water %d", &durata);

        StartWatering(durata);

        char msgBuffer[50];
        snprintf(msgBuffer, sizeof(msgBuffer), "Pompa accesa per %d secondi.", durata);
        bot.sendMessage(CHAT_ID, msgBuffer, "");
      } 
      else if (strcmp(c_text, "/getdata") == 0) {
        // Avviamo la lettura
        GetSoilMoistureSensorMeasurements(sensor1);
        GetSoilMoistureSensorMeasurements(sensor2);
        GetSoilMoistureSensorMeasurements(sensor3);
        GetSoilMoistureSensorMeasurements(sensor4);
        GetSoilMoistureSensorMeasurements(sensor5);
      } else if (strcmp(c_text, "/realtime") == 0) {
        // Avviamo la lettura
        GetAirTempHumiSensorMeasurements();
      } else if (strcmp(c_text, "/stop") == 0) {
        StopWatering();
        StopAndReset();
      }
      else{
        bot.sendMessage(CHAT_ID, "Comando non trovato", "");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void GetSoilMoistureSensorMeasurements(SoilSensor sensor) {
  // Pick 5 measurements and do the average
  float lettura=sensor.leggi(); 

  //DEBUG Serial.println(sensor.getNome()+". "+lettura);
  char message[128]; 

  // 2. Usa snprintf per formattare la stringa e salvarla nel buffer
  snprintf(message, sizeof(message), "📊 %s: %d%%", sensor.getNome(), sensor.getPercentuale());
  bot.sendMessage(CHAT_ID, message, "");
}

void GetAirTempHumiSensorMeasurements() {
  pinMode(VCC_RAIN_SENSOR, HIGH);
  float air_temperature=sensor_dht.leggiTemp();
  //DEBUG Serial.println(temp);
  float air_humidity=sensor_dht.leggiHumi();
  //DEBUG Serial.println(humi);
  
  //Anemometer
  //sviluppato nella funzione void CalcolaVentoOgniMinuto()

  int lettura_RAIN_SENSOR=analogRead(RAIN_SENSOR); //0-1023

  int percentuale_pioggia = map(lettura_RAIN_SENSOR, 4095, 0, 0, 100); //100=asciutto, 0=pioggia forte
    if (percentuale_pioggia<0){
    percentuale_pioggia=0;
  }
  else if(percentuale_pioggia>100){
    percentuale_pioggia=100;
  }
  // 4. Determiniamo l'intensità in base a delle soglie

  // (Puoi modificare questi numeri in base ai tuoi test reali)
  const char* str;
  if (percentuale_pioggia > 85) {
    str="☀️ Asciutto";
  }
  else if (percentuale_pioggia > 70) {
    str="🌦️ Pioviggine / Pioggia leggera";
  }
  else if (percentuale_pioggia > 40) {
    str="🌧️ Pioggia moderata";
  }
  else {
    str="⛈️ Acqua a catinelle!";
  }

  char message[256]; 
  if (isnan(air_temperature) || isnan(air_humidity)) {
  snprintf(message, sizeof(message), 
           "%s\n🌡️ Temperatura: %.1f °C\n💧 Umidità: %.1f %%\n💨 Vento: %.1f km/h \n Condizioni atmosferiche: %s", 
           sensor_dht.getNome(), 
           air_temperature, 
           air_humidity, 
           ultimaVelocitaVentoCalcolata, 
           str);
  }
  else{
      snprintf(message, sizeof(message), 
           "%s\n⚠️ Errore DHT!\n💨 Vento: %.1f km/h \n Condizioni atmosferiche: %s", 
           sensor_dht.getNome(), 
           ultimaVelocitaVentoCalcolata, 
           str);
  }
  bot.sendMessage(CHAT_ID, message, "");
  pinMode(VCC_RAIN_SENSOR, LOW);
}

void StopAndReset() {  //Interrupt what ESP32 is doing and return in "IDLE" state: red button
  bot.sendMessage(CHAT_ID, "🚨 RESET DI EMERGENZA AVVIATO...", "");

  digitalWrite(RELAY_1, LOW);  // Spegne subito la pompa 1
  digitalWrite(RELAY_2, LOW);  // Spegne subito la pompa 2
  watering = false;         // Reset flag irrigazione

  // 2. FERMA I TIMER (Ticker)
  timerPompa.detach();  // Impedisce che il Ticker provi a chiamare StopWatering in futuro

  // 3. SVUOTA LA CODA TELEGRAM
  // Chiediamo a Telegram tutti i messaggi arretrati e NON facciamo nulla.
  // Questo aggiorna l'offset sul server, "bruciando" i comandi vecchi.
  int num_messaggi_arretrati = bot.getUpdates(bot.last_message_received + 1);
  while (num_messaggi_arretrati > 0) {
    bot.last_message_received = bot.messages[num_messaggi_arretrati - 1].update_id;
    num_messaggi_arretrati = bot.getUpdates(bot.last_message_received + 1);
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
    digitalWrite(LED_BUILTIN, (millis() / 100) % 2);
  } else if (watering) {
    digitalWrite(LED_BUILTIN, (millis() / 500) % 2);  // Lampeggio medio durante irrigazione
  } else {
    digitalWrite(LED_BUILTIN, HIGH);  // Fisso se tutto ok
  }
}
void SendEmail(){
  auto statusCallback = [](SMTPStatus status) {
    //Serial.println(status.text);
  };
  smtp.connect(SMTP_HOST, SMTP_PORT, statusCallback);

  if (smtp.isConnected()) {
    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_APP_PASS, readymail_auth_password);

    SMTPMessage msg;

    msg.headers.add(rfc822_from, String(AUTHOR_NAME) + " <" + AUTHOR_EMAIL + ">");
    msg.headers.add(rfc822_to, String(RECIPIENT_NAME) + " <" + RECIPIENT_EMAIL + ">");
    msg.headers.add(rfc822_subject, "Resoconto giornaliero Meteo Station");
    //msg.text.body("This is a plain text message.");


    float temperature_avg = air_temperature_somma/i;
    float humidity_avg = air_humidity_somma/i;
    char htmlMsg[500]; 

    // Compiliamo il testo sostituendo i %f con le tue variabili
    snprintf(htmlMsg, sizeof(htmlMsg), 
      "<html><body style='font-family: Arial;'>"
      "<h2>🌻 Report Irrigazione Giornaliero</h2>"
      "<ul>"
      "<li><b>Temperatura Media:</b> %.1f &deg;C</li>"
      "<li><b>Umidit&a Media:</b> %.1f %%</li>"
      "<li><b>Temperatura massima:</b> %.1f %%</li>"
      "<li><b>Temperatura minima:</b> %.1f %%</li>"
      "</ul>"
      "</body></html>", 
    temperature_avg, humidity_avg, air_temperature_max, air_temperature_min);

  // Assegniamo la stringa creata al corpo della mail
  msg.html.body(htmlMsg);
     
    // Set timestamp for the email
    while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);

    smtp.send(msg);
    i=0; //resetto il numero delle misure giornaliere
    //resetto i valori delle temperature massima e minima giornaliere
    air_temperature_max=TEMP_MIN; //temperatura massima giornaliera
    air_temperature_min=TEMP_MAX; //temepratura minima giornaliera
    air_temperature_somma=0.0;
    air_humidity_somma=0.0;
  }
}

void CheckAndSendDailyEmail() {
  struct tm timeinfo;
  
  // Se non riesce a ottenere l'ora, esce (non è connesso o non ha ancora sincronizzato)
  if(!getLocalTime(&timeinfo)){
    return;
  }

  // Variabile statica: ricorda l'ultimo giorno in cui ha mandato la mail
  static int lastDaySent = -1;

  // Se sono le 21, i minuti sono 0 (o poco più), e non abbiamo ancora inviato oggi:
  if (timeinfo.tm_hour == 21 && timeinfo.tm_min == 0 && timeinfo.tm_mday != lastDaySent) {
    
    //Serial.println("Sono le 21:00! Invio il resoconto giornaliero...");
    
    SendEmail(); // Chiama la tua funzione
    
    // Aggiorna il giorno, così non rimanda la mail 100 volte in questo minuto
    lastDaySent = timeinfo.tm_mday; 
    
    // QUI DOVRESTI AZZERARE LE TUE VARIABILI GIORNALIERE (es. tempMedia = 0)
  }
}
void InviaDatiThingSpeak(){
  if (WiFi.status() == WL_CONNECTED) {
    //Serial.println("Inizio invio dati a ThingSpeak...");
    HTTPClient http;  

  // Your Domain name with URL path or IP address with path
      http.begin(ThingSpeak_serverName);
      http.addHeader("Content-Type", "application/x-www-form-urlencoded");
      float air_temperature = sensor_dht.leggiTemp();
      float air_humidity = sensor_dht.leggiHumi();

      air_temperature_somma+=air_temperature;
      air_humidity_somma+=air_humidity;
      i++;

      if(air_temperature<air_temperature_min){
        air_temperature_min=air_temperature;
      }
      else if(air_temperature>air_temperature_max){
        air_temperature_max=air_temperature;
      }

      char httpRequestData[256];
      snprintf(httpRequestData, sizeof(httpRequestData), "api_key=%s&field1=%f&field2=%f", ThingSpeak_apiKey, air_temperature, air_humidity);   
      // Send HTTP POST request
      int httpResponseCode = http.POST(httpRequestData);
      if (httpResponseCode > 0) {
        //Serial.print("✅ Dati inviati! Codice HTTP: ");
        //Serial.println(httpResponseCode);
      } else {
        //Serial.print("❌ Errore durante l'invio HTTP: ");
        //Serial.println(httpResponseCode);
    }
      // Free resources
      http.end();
  }
else {
    //Serial.println("Impossibile inviare a ThingSpeak: WiFi Disconnesso");
  }
}
void CalcolaVentoOgniMinuto() {
  unsigned long tempoAttuale = millis();
  // Controlla se sono passati 60000 millisecondi (1 minuto)
  if (tempoAttuale - ultimoAzzeramentoVento >= 60000) {
    
    // 1. Lettura sicura degli impulsi
    noInterrupts(); 
    int impulsi_minuto = contatoreImpulsiVento; 
    contatoreImpulsiVento = 0; // Azzera per il prossimo minuto           
    interrupts(); 

    // 2. Calcolo dei Giri al Secondo (Hertz)
    // Dato che sappiamo che è passato esattamente un minuto, dividiamo per 60
    float giri_sec = impulsi_minuto / 60.0;
    
    // 3. Formula di conversione in km/h (Usa il TUO fattore di calibrazione!)
    ultimaVelocitaVentoCalcolata = giri_sec * 2.4; 

    // DEBUG opzionale:
    // Serial.print("Vento medio nell'ultimo minuto: ");
    // Serial.println(ultimaVelocitaVentoCalcolata);

    // Riavvia il cronometro del minuto
    ultimoAzzeramentoVento = tempoAttuale;
  }
}