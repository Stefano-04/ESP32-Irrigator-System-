#include "MyHeader.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>           //Handle Telegram messages, separating all the informations

const float TEMP_MAX=60.0;
const float TEMP_MIN=-30.0;

int j=0;//numero di misure già fatte nel corso della giornata (air_temp, air_humi) per il report giornaliero via mail
//Important infos displayed on Telegram chat when requested

const char* Startup_Menu_Telegram =
  "Commands:\n\n"
  "/menu@stazione_meteo2_bot - send this menu\n"
  "/stop@stazione_meteo2_bot - stop watering and delete Telegram pending messages\n"
  "/water X Y- activate water pump n.X for Y seconds\n"
  "/realtime_air@stazione_meteo2_bot - get real-time measurements from air sensor\n"
  "/realtime_soil@stazione_meteo2_bot - get real-time measurements from soil moisture sensors";

float air_temperature_somma=0.0; //vettore temperatura giornaliere
float air_humidity_somma=0.0;    //vettore umidità giornaliere
float air_temperature_max=TEMP_MIN; //temperatura massima giornaliera
float air_temperature_min=TEMP_MAX; //temepratura minima giornaliera

float temperature_avg;
float humidity_avg;

volatile int contatoreImpulsiVento = 0;
unsigned long ultimoAzzeramentoVento = 0; // Timer per il minuto
float ultimaVelocitaVentoCalcolata = 0.0; // Memorizza l'ultimo calcolo valido
volatile unsigned long ultimoTempoInterrupt=0;

int num_pompa=0;

void IRAM_ATTR ContaImpulsi() { //ISR per il conteggio del sensore Hall
   unsigned long tempoAttuale = millis();
  
  // Se sono passati almeno 15 ms dall'ultimo impulso, consideralo valido
  if (tempoAttuale - ultimoTempoInterrupt > 15) {
    contatoreImpulsiVento++; 
    ultimoTempoInterrupt = tempoAttuale;
  }
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
    bot.sendMessage(GROUP_ID, "✅ WiFi connected", "");
    bot.sendMessage(GROUP_ID, Startup_Menu_Telegram, "");
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

void StartWatering(int num_pompa, int seconds) {
  if (seconds <= 0 || seconds > 60) {
    seconds = 10; //Imposto un valore basso in caso in cui ci sia un errore
  }
  //Avvia l'irrigazione (HIGH->POMPA FUNZIONANTE)
  if(num_pompa==1){
    digitalWrite(RELAY_1, HIGH);
    watering_pump1=true;
  }
  else if(num_pompa==2){
    digitalWrite(RELAY_2, HIGH);
    watering_pump2=true;
  }
  timerPompa.once(seconds, StopWatering);  // Setup per lo Spegnimento
}

void StopWatering() {
  //Se la pompa 1 era in watering (watering_pump1) o arriva uno stop forzato (watering_reset), allora stopWatering
  if(watering_pump1 || watering_reset){
    watering_pump1=false;
    digitalWrite(RELAY_1, LOW);
    bot.sendMessage(GROUP_ID, "Irrigazione pompa n.1 completata con successo!", "");
  }
  if(watering_pump2|| watering_reset){
    watering_pump2=false;
    digitalWrite(RELAY_2, LOW);
     bot.sendMessage(GROUP_ID, "Irrigazione pompa n.2 completata con successo!", "");
  }
  watering_reset=false;
  num_pompa=0;
}

void HandleNewMessages() {
  int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  while (numNewMessages) {
    for (int i = 0; i < numNewMessages; i++) {
      const char* c_text = bot.messages[i].text.c_str();
      String from_id = String(bot.messages[i].chat_id);

      if (from_id != GROUP_ID) {
        bot.sendMessage(from_id, "Utente non autorizzato.", "");
        continue;
      }

      if (strcmp(c_text, "/menu@stazione_meteo2_bot") == 0) {
        bot.sendMessage(GROUP_ID, Startup_Menu_Telegram, "");
      } 
      else if (strncmp(c_text, "/water", 6) == 0) {
        int durata = 10; 
        
        sscanf(c_text, "/water %d %d", &num_pompa, &durata);

        StartWatering(num_pompa,durata+11); //Aggiungo 11 secondi circa per permettere alla pompa di accendersi ed essere pronta a bagnare

        char msgBuffer[50];
        snprintf(msgBuffer, sizeof(msgBuffer), "Pompa n. %d accesa per %d secondi.", num_pompa, durata);
        bot.sendMessage(GROUP_ID, msgBuffer, "");
      } 
      else if (strcmp(c_text, "/realtime_soil@stazione_meteo2_bot") == 0) {
        // Avviamo la lettura
        GetSoilMoistureSensorMeasurements(sensor1);
        GetSoilMoistureSensorMeasurements(sensor2);
        GetSoilMoistureSensorMeasurements(sensor3);
        GetSoilMoistureSensorMeasurements(sensor4);
        GetSoilMoistureSensorMeasurements(sensor5);
      } else if (strcmp(c_text, "/realtime_air@stazione_meteo2_bot") == 0) {
        // Avviamo la lettura
        GetAirTempHumiSensorMeasurements();
      } else if (strcmp(c_text, "/stop@stazione_meteo2_bot") == 0) {
        watering_reset=true; //setto flag di reset watering per blocco forzato pompe
        StopWatering();
        StopAndReset();
      }
      else{
        bot.sendMessage(GROUP_ID, "Comando non trovato", "");
      }
    }
    numNewMessages = bot.getUpdates(bot.last_message_received + 1);
  }
}

void GetSoilMoistureSensorMeasurements(SoilSensor sensor) {
  //DEBUG Serial.println(sensor.getNome()+". "+lettura);
  char message[128]; 

  // 2. Usa snprintf per formattare la stringa e salvarla nel buffer
  snprintf(message, sizeof(message), "📊 %s: %d%%", sensor.getNome(), sensor.getPercentuale());
  bot.sendMessage(GROUP_ID, message, "");
}

void GetAirTempHumiSensorMeasurements() {
  digitalWrite(VCC_RAIN_SENSOR, HIGH);
  float air_temperature=sensor_dht.leggiTemp();
  //DEBUG Serial.println(temp);
  float air_humidity=sensor_dht.leggiHumi();
  //DEBUG Serial.println(humi);
  
  //Anemometer
  //sviluppato nella funzione void CalcolaVentoOgniMinuto()

  int lettura_RAIN_SENSOR=analogRead(RAIN_SENSOR); //0-14095

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
  if (percentuale_pioggia > 90) {
    str="☀️ Asciutto";
  }
  else if (percentuale_pioggia > 80) {
    str="🌦️ Pioviggine / Pioggia leggera";
  }
  else if (percentuale_pioggia > 70) {
    str="🌧️ Pioggia moderata";
  }
  else {
    str="⛈️ Acqua a catinelle!";
  }

  char message[256]; 
  if (isnan(air_temperature) || isnan(air_humidity)) {
        snprintf(message, sizeof(message), 
           "%s\n⚠️ Errore DHT!\n💨 Vento: %.1f km/h \n Condizioni atmosferiche: %s", 
           sensor_dht.getNome(), 
           ultimaVelocitaVentoCalcolata, 
           str);
  }
  else{
      snprintf(message, sizeof(message), 
           "%s\n🌡️ Temperatura: %.1f °C\n💧 Umidità: %.1f %%\n💨 Vento: %.1f km/h \n Condizioni atmosferiche: %s", 
           sensor_dht.getNome(), 
           air_temperature, 
           air_humidity, 
           ultimaVelocitaVentoCalcolata, 
           str);
  }
  bot.sendMessage(GROUP_ID, message, "");
  digitalWrite(VCC_RAIN_SENSOR, LOW);
}

void StopAndReset() {  //Interrupt what ESP32 is doing and return in "IDLE" state: red button
  bot.sendMessage(GROUP_ID, "🚨 RESET DI EMERGENZA AVVIATO...", "");

  digitalWrite(RELAY_1, LOW);  // Spegne subito la pompa 1
  digitalWrite(RELAY_2, LOW);  // Spegne subito la pompa 2
  watering_pump1 = false;         // Reset flag irrigazione pompa 1
  watering_pump2 = false;         // Reset flag irrigazione pompa 2
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
  bot.sendMessage(GROUP_ID, "🚨 Sistema resettato. Pompa spenta e comandi pendenti cancellati.", "");

  // OPZIONALE: Se vuoi un vero "ripartire da zero" fisico
  // Serial.println("Riavvio fisico dell'ESP32...");
  // ESP.restart();
}


void SendEmail(){
  auto statusCallback = [](SMTPStatus status) {
    //Serial.println(status.info());
  };
  smtp.connect(SMTP_HOST, SMTP_PORT, statusCallback);

  if (smtp.isConnected()) {
    smtp.authenticate(AUTHOR_EMAIL, AUTHOR_APP_PASS, readymail_auth_password);

    SMTPMessage msg;

    msg.headers.add(rfc822_from, String(AUTHOR_NAME) + " <" + AUTHOR_EMAIL + ">");
    msg.headers.add(rfc822_to, String(RECIPIENT_NAME) + " <" + RECIPIENT_EMAIL + ">");
    msg.headers.add(rfc822_subject, "Resoconto giornaliero Meteo Station");
    //msg.text.body("This is a plain text message.");

    if (j==0){//Impedisco divisioni per zero nei calcoli successivi-->possibile fonte di crash
      j++;
    }
    temperature_avg = (float) air_temperature_somma/j;
    humidity_avg = (float) air_humidity_somma/j;
    char htmlMsg[500]; 

    // Compiliamo il testo sostituendo i %f con le tue variabili
    snprintf(htmlMsg, sizeof(htmlMsg), 
      "<html><body style='font-family: Arial;'>"
      "<h2>🌻 Report Irrigazione Giornaliero</h2>"
      "<ul>"
      "<li><b>Temperatura Media:</b> %.1f &deg;C</li>"
      "<li><b>Umidit&agrave; Media:</b> %.1f %%</li>"
      "<li><b>Temperatura massima:</b> %.1f &deg;C</li>"
      "<li><b>Temperatura minima:</b> %.1f &deg;C</li>"
      "<li><b>Misure effettuate: </b> %d </li>"
      "</ul>"
      "</body></html>", 
    temperature_avg, humidity_avg, air_temperature_max, air_temperature_min, j);

  // Assegniamo la stringa creata al corpo della mail
  msg.html.body(htmlMsg);
     
    // Set timestamp for the email
    //while (time(nullptr) < 100000) delay(100);
    msg.timestamp = time(nullptr);
    if (!smtp.send(msg)) {
 //      Serial.println("Errore invio mail: " + smtp.errorReason());
    } else {
    //   Serial.println("Email inviata con successo!");
    }
    smtp.send(msg);

    j=0; //resetto il numero delle misure giornaliere
    //resetto i valori delle temperature massima e minima giornaliere
    air_temperature_max=TEMP_MIN; //temperatura massima giornaliera
    air_temperature_min=TEMP_MAX; //temepratura minima giornaliera
    air_temperature_somma=0.0;
    air_humidity_somma=0.0;
  }
  else {
    //Serial.println("Impossibile connettersi al server SMTP.");
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
  if (timeinfo.tm_hour == 23 && timeinfo.tm_min == 59 && timeinfo.tm_mday != lastDaySent) {
    
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
      j++;
      
      if(air_temperature<air_temperature_min){
        air_temperature_min=air_temperature;
      }
      if(air_temperature>air_temperature_max){
        air_temperature_max=air_temperature;
      }

      char httpRequestData[256];
      snprintf(
      httpRequestData, 
      sizeof(httpRequestData), 
      "api_key=%s&field1=%.1f&field2=%.1f&field3=%.1f&field4=%d&field5=%d&field6=%d&field7=%d&field8=%d",
      ThingSpeak_apiKey, 
      air_temperature,
      air_humidity, 
      ultimaVelocitaVentoCalcolata, 
      sensor1.getPercentuale(), 
      sensor2.getPercentuale(), 
      sensor3.getPercentuale(), 
      sensor4.getPercentuale(), 
      sensor5.getPercentuale()
      );   
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