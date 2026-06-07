# ESP32-Irrigator-System-
ESP32 Irrigator System 
Controllo remoto dell’irrigazione e lettura sensori tramite chat Telegram

📌 Descrizione

Questo progetto permette di controllare tramite ESP32 e un Bot Telegram, un impianto di irrigazione DIY, con la possibilità di:
- avviare l'irrigazione impostandone la durata in secondi, attraverso il power-on reset di due pompe elettriche.
- misurare l'umidità del terreno attraverso dei sensori analogici capacitivi.
- misurare l'umidità e la temperatura atmosferica attraverso un sensore digitale DHT11.
- *Prossimamente: circuito di misura carica della batteria, sensore rilevamento pioggia, sensore rilevamento acqua rimanente nel serbatoio*

Alimentazione
-
  Il circuito è interamente alimentato con un pannello solare da (*dimensioni e specifiche*) che ricarica una batteria al Piombo-Acida ("Lead Acid Battery") per auto da 12V (*inserire caratteristiche*). La batteria alimenta le due pompe a 5V e la scheda ESP32 a 3.3V grazie all'utilizzo di convertitori step-down. Tutti i sensori sono alimentati dai 5V forniti dal primo regolatore.
  
  *Inserire schema elettrico*


  Le pompe elettriche sono pilotate da un relay a 5V controllate da un segnale di output dell'ESP.

  
Funzionamento HW
-
L'utente può controllare e monitorare l'irrigazione e leggere i dati dei sensori tramite un semplice messaggio nella chat in cui è presente il bot. 
Quando riceve un messaggio, l'ESP32 esegue il codice contenente le istruzioni per quel particolare comando. 
I comandi ammessi sono:
- *Inserire comandi disponibili con descrizione*
- _/water X_
  Permette l'avvio dell'irrigazione per i prossimi _X_ secondi. L'utente riceverà alcuni messaggi di feedback nel momento dell'inizio e della fine dell'irrigazione.

Funzionamento SW
-
*Descrizione funzionamento bot Telegram e lettura comdandi via chat*

Componenti utilizzati
-
4x Capacitive soil moisture sensor
1x Sensore temperatura DHT11
*1x Sensore pioggia*
*1x Sensore di profondità IR*
2x Moduli Relay a 5V
1x Pannello Solare con controller
1x Batteria VARTA 12V 52Ah
1x Scheda ESP32
2x Pompe elettriche a 5V

Modifiche rispetto alla versione precedente
-
E' stato rivestito accuratamente il circuito elettronico del sensore di umidità del terreno per prevenire arruginimenti e corto-circuiti.

Per poter riutilizzare il codice è necessario allegare un file header "Login.h" nella stessa cartella del progetto, dove scrivere SSID e PASSWORD della propria rete fissa e il token e il chat-id del proprio Bot telegram (dati privati).
