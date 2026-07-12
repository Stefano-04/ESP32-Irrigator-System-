# ESP32-Irrigator-System-
ESP32 Irrigator System 
Controllo remoto dell’irrigazione e lettura sensori tramite chat Telegram

📌 Descrizione

Questo progetto permette di controllare tramite ESP32 e un Bot Telegram, un impianto di irrigazione DIY, con la possibilità di:
- avviare l'irrigazione impostandone la durata in secondi, attraverso il power-on reset di due pompe elettriche, gestito da due relay elettromeccanici.
- misurare l'umidità del terreno attraverso dei sensori analogici capacitivi.
- misurare l'umidità e la temperatura atmosferica attraverso un sensore digitale DHT11.
- misurare la velocità del vento attraverso un anemometro fai-da-te costruito con sensore a effetto hall.
- misurare le precipitazioni atmosferiche attraverso un sensore di pioggia resistivo.
- *Prossimamente: circuito di misura carica della batteria, sensore rilevamento acqua rimanente nel serbatoio*

Alimentazione
-
  Il circuito è interamente alimentato con un pannello solare da (*dimensioni e specifiche*) che ricarica una batteria al Piombo-Acida ("Lead Acid Battery") per auto da 12V (*inserire caratteristiche*). 
  L'energia della batteria viene gestita da un convertitore step-down DC-DC: esso riduce la tensione a 5V per alimentare le due pompe elettriche, la scheda ESP32 e la basetta millefori alla quale sono collegati i sensori capacitivi, il sensore hall, il sensore di pioggia e i comandi per i due relay. Tutti i sensori di lettura operano a 3.3V per garantire la compatibilità con gli ingressi dell'ESP32 e preservarne i pin, consentendo ai sensori analogici una corretta conversione A-D (il convertitore ADC dell'ESP32 è a 12 bit, quindi range 0-4095).
  
  *Inserire schema elettrico*


  Le pompe elettriche sono pilotate da un relay a 5V controllate da un segnale di output dell'ESP.

  
Funzionamento HW
-
L'utente interagisce con l'impianto inviando semplici comandi testuali sulla chat di Telegram. Quando l'ESP32 riceve e decodifica un messaggio valido, esegue la routine associata (es. accensione del relay della pompa o lettura dei sensori) e invia un messaggio di feedback all'utente. L'utente può quindi controllare e monitorare l'irrigazione e leggere i dati dei sensori tramite un semplice messaggio nella chat in cui è presente il bot. 
Quando riceve un messaggio, l'ESP32 esegue il codice contenente le istruzioni per quel particolare comando. 
I comandi ammessi sono:
- _/start
  Invia di nuovo il menù informativo con tutti i comandi
- *Inserire comandi disponibili con descrizione*
- _/water X_
  Permette l'avvio dell'irrigazione per i prossimi _X_ secondi. L'utente riceverà alcuni messaggi di feedback nel momento dell'inizio e della fine dell'irrigazione.
-_/realtime_ 
  Richiede la lettura istantanea della temperatura e umidità dell'aria (DHT11).

-_/getdata_ 
  Restituisce la percentuale di umidità del terreno rilevata dai sensori capacitivi.

-_/stop_ 
  Interrompe immediatamente qualsiasi operazione in corso (es. spegne le pompe).

*Trasferimento e analisi dei dati su ThingSpeak*
Ogni 30 minuti il sistema legge i dati di temperatura e umidità dell'aria e li invia a un server ThingSpeak per realizzare dei grafici giornalieri di temperatura e umidità nel corso della giornata.


*Descrizione funzionamento bot Telegram e lettura comdandi via chat*

Componenti utilizzati
-
4x Sensori capacitivi di umidità del terreno
1x Sensore di temperatura DHT11
1x Sensore di pioggia resistivo
*1x Sensore di profondità IR*
1x Sensore a effetto Hall
2x Moduli Relay a 5V
1x Pannello Solare con Power Controller
1x Batteria VARTA 12V 52Ah
1x Scheda ESP32
2x Pompe elettriche a 5V 2W

Note sulla Sicurezza e Installazione
-

**Impermeabilizzazione**: I componenti elettronici dei sensori capacitivi sono stati accuratamente rivestiti con termorestringente per prevenire ossidazione, ruggine e possibili cortocircuiti dovuti al contatto prolungato con il terreno umido e l'acqua piovana.

**Privacy**: Per ragioni di sicurezza, le credenziali della rete Wi-Fi (SSID e PASSWORD) e le chiavi del bot Telegram (TOKEN e CHAT_ID) non sono incluse nel codice principale. Devono essere inserite dall'utente creando un file header denominato MyLogin.h nella stessa cartella dello sketch.

**Power Management** Per ridurre al minimo il rischio di corrosione delle piste di rame, ridurre i consumi e aumentare il tempo di vita si è scelto di alimentare il sensore di pioggia solo durante il momento della misura e lasciarlo spento il resto del tempo. 

**Stop di Emergenza** E' stato aggiunto uno stato di "Stop" per evitare situazioni anomale e riportare il sistema in RESET.
