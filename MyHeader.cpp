#include "MyHeader.h"

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
        bot.sendMessage(CHAT_ID, Startup_Menu_Telegram, "");
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

void GetSoilMoistureSensorMeasurements(SoilSensor s) {
  // Pick 5 measurements and do the average
  float lett_temp=s.leggi(); 

  Serial.println(s.getNome()+". "+lett_temp);

  String messaggio = "📊 " + s.getNome() + ": " + String(s.getPercentuale()) + "%";
  bot.sendMessage(CHAT_ID, messaggio, "");
}

void GetAirTempHumiSensorMeasurements() {
  String msg = clima.getNome()+" \n";
  float temp=clima.leggiTemp();
  Serial.println(temp);
  float humi=clima.leggiHumi();
  Serial.println(humi);
    msg += "Temperatura: " + String(temp) + " °C\n";
    msg += "Umidità: " + String(humi) + " %";
    int sensorValue = digitalRead(HALL_SENSOR_PIN); // Read sensor output
    Serial.println(sensorValue);
    msg+= "Vento"+ String(sensorValue);
    bot.sendMessage(CHAT_ID, msg, "Markdown");
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