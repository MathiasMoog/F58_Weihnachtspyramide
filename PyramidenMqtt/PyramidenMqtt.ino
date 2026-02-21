/* Pyramdiden Steuerung mit MQTT

  Siehe ReadMe.md im Übergeordneten Ordner.

  Hardware:

  - Taster nach GND an PIN_TA (12)
  - Motortreiber Transistor an PIN_M (PWM, D9)
  - LEDs an den Pins D2 bis D7 im pins_led Array

  Ich habe ein altes "Prezel-Board" https://iot.fkainka.de/ von F. Kainka verwendet. 
  Zum Programmieren Nano und "Old Bootloader" anwählen.

  Software:

  Der Sketch ist in die Abschnitte
   - Kerzen
   - Motor
   - Taster 
   - MQTT 
  gegliedert.

  Die WiFi und MQTT Zugangsdaten sind in der externen secrets.h Datei.
  Eingecheckt ist nur eine leere Vorlage. Umbenennen in secrets.h und auffüllen!

  Copyright, 2026, Mathias Moog, Deutschland, CC-BY-NC-SA
*/

// -------------------------------------------------------------------------
// Genutzte Bibliotheken und lokale Header

// PWM in Software, https://github.com/bhagman/SoftPWM
#include <SoftPWM.h>
// Prezel Board Bibliothek für WLAN und MQTT, https://github.com/FKainka/NanoESP
// Nicht im Arduino Bibliotheksmanager, in das Arduino library Verzeichnis herunterladen / clonen
// Die Bibliothek ist schon betagt, es entstehen einige Warnungen beim Übersetzen
// Aktuell Bugfix in meinem Branch: https://github.com/MathiasMoog/NanoESP
#include <NanoESP.h>
#include <NanoESP_MQTT.h>
#include <SoftwareSerial.h>
// Lokale header Datei mit den Zugangsdaten und den MQTT Topics
// Eingecheckt ist nur die Vorlage secrets_vorlage.h. Umgenennen und anpassen!
#include "secrets.h"


// Loop Counter, einige aufwändige Aufgabe nur sporadisch ausführen
byte lc = 0;


/*--------------------------------------------------------------------------
  Flackernde LEDs als Kerzenersatz.

  Basierend auf der SoftPWM Bibliothek von Brett Hagman und Paul Stoffregen, Siehe https://github.com/bhagman/SoftPWM

  Es werden fortlaufende Pins, hier 2 bis 7 verwendet. Das vereinfacht den Aufbau und den Code.
*/

// LEDs von einschließlich pin_start bis einschließlich pin_end
const byte pin_start = 2;
const byte pin_end = 7;
// Anzahl der LEDs
const byte npins = pin_end - pin_start + 1;
// Loop Counter (lc) für das nächste Flackern
byte flickerLc[npins];
// Schalter zum An- und Ausschalten der Kerzen
boolean candleState = false;
// Alter Zustand des Schalters. Wird für die MQTT Status Rückmeldungen benötigt
boolean new_candleState = candleState;

// Initialisierung der Kerzen Pins und Variablen
void setupCandle() {
  for (byte i = 0; i < npins; i++) {
    SoftPWMSet(pin_start + i, 0); // Aus
    flickerLc[i] = lc + random(1, 6); // Flicker
  }
}

// Lasse die Kerzen mit flackern beginnen. Zu Beginn alle gleichzeitig mit Zufallswerten an.
void startFlicker() {
  for (byte i = 0; i < npins; i++) {
    const byte pwm = random(50, 256);
    SoftPWMSet(pin_start + i, pwm);
    flickerLc[i] = lc + random(1, 6);
  }
}

// Flackern, nur die für die flickerLc zugrifft. Prüfe candle State und schalte aus.
void flickerCandle() {
  if (candleState) {
    for (byte i = 0; i < npins; i++) {
      if (lc == flickerLc[i]) {
        const byte pwm = random(50, 256);
        SoftPWMSet(pin_start + i, pwm);
        flickerLc[i] = lc + random(1, 6);
      }
    }
  } else {
    // Switch off
    for (byte i = pin_start; i <= pin_end; i++) {
      SoftPWMSet(i, 0);
    }
  }
}

/* -----------------------------------------------------------------------------
    Antrieb der Pyramide, PWM Steuerung

    Nutzt Hardware PWM auf D9
*/

#define PIN_M 9

// Initialisierung, Motor ist aus
void setupMotor() {
  pinMode(PIN_M, OUTPUT);
  digitalWrite(PIN_M, LOW);
}

/* Ansteuerung des Motors
  Der Motor läuft erst ab ca. 2,5 V an. 

  Lösung: Über MQTT kommt von openHAB der Dimmwert als Prozent (0 bis 100)
  
  Die 0 geht auf die 0 über alle anderen Zahlen werden um 155 erhöht.
*/
void setMotor(byte percent) {
  if (percent == 0) {
    digitalWrite(PIN_M, LOW);
  } else {
    analogWrite(PIN_M, constrain(155 + percent, 0, 255));
  }
}

/* -----------------------------------------------------------------------------
    Der Taster für den manuellen Wechel der Modi

    Noch nicht fertig programmiert ...
*/
#define PIN_TA 10
// Letzter Zustand. Entprellen automatisch durch den langsamen Loop Durchlauf.
boolean ta_last_state = HIGH;

// Setup des Taster Pins
void setupTaster() {
  pinMode(PIN_TA, INPUT_PULLUP);
  ta_last_state = digitalRead(PIN_TA);
}


// Liefert true Wenn der Taster neu gedrückt wurde
boolean checkTaster() {
  const boolean s = digitalRead(PIN_TA);
  boolean ret = (s == LOW && ta_last_state == HIGH);
  ta_last_state = digitalRead(PIN_TA);
  return ret;
}


/* -----------------------------------------------------------------------------
    MQTT Anbindung

    Siehe:  https://github.com/FKainka/NanoESP/tree/master/examples/MQTT/MQTT_Basics

  Benötigt eine aktuelle Firmware. Mein altes Brezel Board hatte eine zu alte Firmware.

  Der ESP Chip ist über Software Serial an den Pins 11 und 12 angebunden.

*/

// Zugriff auf das WiFi Modul mit dem ESP
NanoESP nanoesp = NanoESP();
// MQTT für den Nano ESP
NanoESP_MQTT mqtt = NanoESP_MQTT(nanoesp);


// Eingebaute LED, signalisiert die WiFi und MQTT Verbindung. An wenn alles OK ist
const byte pin_mqtt_led = 13;
/* State: 0=b00-off (Anfangszustand), 1=b01-Nur Kerzen, 2=b10-Nur Motor, 3=b11-Kerzen und Motor
 Die Wahl ermöglich einen Abbleich mit MQTT durch bit 0 Kerzen und bit 1 Motor 
*/
byte state = 0;
// Neuer Zustand, in loop erfolgt der Zustandswechsel
byte new_state = state;
// Motor Drehzahl Einstellung mit akutellem Zustand und neuem Wert
byte motor = 100;
byte new_motor = motor;

// In den nächsten Zustand wecheln
// Wird in Verbindung mit dem Taster genutzt. Mit jedem Tastendruck der nächste Zustand
// Wenn alle durch sind beginnt es wieder von vorne.
void nextState() {
  new_state = state + 1;
  if (new_state > 3) {
    new_state = 0;
  }
}

// Motor Zustand senden, auf dem state topic
boolean publish_motor() {
  Serial.print(F("MQTT sende Motor "));
  Serial.println(motor);
  return mqtt.publish(0, TOPIC_MOTOR_STATE, String(motor));
}

// Kerzen Zustand senden, auf dem state topic
boolean publish_kerzen() {
  Serial.print(F("MQTT sende Kerzen "));
  Serial.println(candleState);
  return mqtt.publish(0, TOPIC_KERZEN_STATE, candleState ? F("ON") : F("OFF"));
}

// Callback Funktion, neue Motor Einstellungen empfangen
void callback_motor(const String& value) {
  // Wandle in zulässige Zahl um
  new_motor = constrain(value.toInt(), 0, 100);
  Serial.print(F("MQTT empfange Motor "));
  Serial.println(new_motor);
  // Die Verarbeitung erfolgt in loop, das vermeidet Hänger mit verschachtelten Funktionsaufrufen
}

// Callback Funktion, neue Kerzen Einstellungen empfangen
void callback_kerzen(const String& value) {
  // ON -> true, OFF -> false
  new_candleState = value.equals(F("ON"));  // Alles was nicht ON ist ist OFF ...
  Serial.print(F("MQTT empfange Kerzen "));
  Serial.println(new_candleState);
  // Die Verarbeitung erfolgt in loop, das vermeidet Hänger mit verschachtelten Funktionsaufrufen
}

// WIFI und MQTT Verbindung aufbauen
// Reconnect ist schwierig ...
void connect() {
  nanoesp.configWifiStation(SECRET_SSID, SECRET_PASS);
  if (nanoesp.wifiConnected()) {
    Serial.println(F("Wifi verbunden"));
    digitalWrite(pin_mqtt_led, HIGH);
    // Gib die IP Adresse aus (nur zu Debug Zwecken)
    Serial.print(F("Zugewiesene IP Adresse "));
    Serial.println(nanoesp.getIp());
  } else {
    Serial.println(F("Wifi nicht verbunden"));
    digitalWrite(pin_mqtt_led, LOW);
    return;
  }

  // MQTT Verbindung aufbauen, Startwerte in die status Felder übermitteln und die set Felder abbonieren
  // Das erste Argument ist die ID, genau wie im Beispiel zur NanoESP Bibliothek (MQTT_Basics.ino) auf 0 gesetzt.
  bool mqtt_ok = true;
  byte mqtt_trials = 0;
  do {
    mqtt_ok = true;
    if (mqtt.connect(0, MQTT_IP, MQTT_PORT, MQTT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println(F("MQTT erfolgreich verbunden"));
      mqtt_ok &= publish_motor();
      mqtt_ok &= publish_kerzen();
      delay(100);
      // Das dritte Argument ist qos ich habe es auf 2 gesetzt, a bei 0 zu viele Verluste stattfanden. 
      if (!mqtt.subscribe(0, TOPIC_MOTOR_SET, 2, callback_motor)) {
        Serial.println(F("Subscribe Fehler für Motor"));
        mqtt_ok = false;
      }
      delay(100);
      if (!mqtt.subscribe(0, TOPIC_KERZEN_SET, 2, callback_kerzen)) {
        Serial.println(F("Subscribe Fehler für Kerzen"));
        mqtt_ok = false;
      }
    } else {
      Serial.println(F("MQTT Verbindungsproblem."));
      mqtt.disconnect(0);
      delay(1000);
      mqtt_ok = false;
    }
  } while (!mqtt_ok & (mqtt_trials < 5));
  // Signalisiere den Verbindungsstatus
  digitalWrite(pin_mqtt_led, mqtt_ok);
  // Trenne die Verbindung falls es Probleme gibt
  if (!mqtt_ok) {
    mqtt.disconnect(0);
    delay(1000);
  }
}

// Initialisierung von WIFI und MQTT, nur einmal in setup
void setupMQTT() {
  // LED aktivieren
  pinMode(pin_mqtt_led, OUTPUT);
  digitalWrite(pin_mqtt_led, LOW);
  // Initialisiere den esp chip
  nanoesp.init();
  // Reduziere das Timeout der zugrundeliegenden Stream Klasse
  nanoesp.setTimeout(250);
  // Verbinde WLAN und MQTT
  connect();
}


/* -----------------------------------------------------------------------------
    Der üblich Sketch Aufbau mit setup und Loop

    Ist durch die vorangestellten Funktionen recht einfach gehalten.

*/

void setup() {
  // Serielle Debug Ausgaben
  Serial.begin(19200);
  Serial.println(F("Weihnachtspyramide mit MQTT Anbindung"));
  // Initialisiere alle Komponenten
  setupMotor();
  setupTaster();
  SoftPWMBegin();
  setupCandle();
  setupMQTT();
}


void loop() {
  // Nur in setup hatte es nicht geholfen. Setze das timeout immer wieder zurück.
  nanoesp.setTimeout(150);
  // Prüfe hin und wieder die WLAN Verbindung, die Prüfung stört den MQTT Empfang.
  if (lc == 237) {
    Serial.println(F("\nCheckWifi"));
    if (!nanoesp.wifiConnected()) {
      Serial.println(F("Reconnect WiFi"));
      connect();
    }
  }
  // Prüfe den Taster
  if (checkTaster()) {
    nextState();
  }
  // Zustandswechsel anhand es Tasters vormerken
  if (new_state != state) {
    Serial.print(F("Wechsel in Zustand "));
    Serial.println(new_state);
    switch (new_state) {
      case 0:  // Schalte alles aus
        new_candleState = false;
        new_motor = 0;
        break;
      case 1:  // Nur Kerzen
        new_candleState = true;
        new_motor = 0;
        break;
      case 2:  // Nur Motor
        new_candleState = false;
        if (motor == 0) {
          new_motor = 100;  // Default ...
        }
        break;
      case 3:  // Kerzen und Motor
        new_candleState = true;
        if (motor == 0) {
          new_motor = 100;  // Default ...
        }
        break;
    }
    state = new_state;
  }

  // Zustandswechsel der Kerzen abarbeiten
  if (candleState != new_candleState) {
    Serial.print(F("Neuer Zustand Kerzen "));
    Serial.println(new_candleState);
    // Akzeptiere den Wechsel, Flackern erfolgt automatisch
    candleState = new_candleState;
    // Kerzen starten
    if (candleState) {
      startFlicker();
    }
    // Aktualisiere MQTT
    publish_kerzen();
    // Aktualisiere den Taster Zustand, vermeide unnötiges doppeltes Update
    bitWrite(state, 0, candleState);
    new_state = state;
  }

  // Zustandswechsel des Motors abarbeiten
  if (motor != new_motor) {
    Serial.print(F("Neuer Zustand Motor "));
    Serial.println(new_motor);
    // Akzeptiere den Wechsel
    motor = new_motor;
    // Setze den Zustand
    setMotor(motor);
    // Aktualisiere MQTT
    publish_motor();
    // Aktualisiere den Taster Zustand, vermeide unnötiges doppeltes Update
    bitWrite(state, 1, motor > 0);
    new_state = state;
  }

  // regelmäßige Aufgaben abarbeiten
  flickerCandle();  // Lass die Kerzen flackern falls gewünscht
  // Nachrichten Empfangen und die passendne Funktionen aufrufen
  // Sehr knauserig mit dem buffer sein. Die Implementierung in NanoESP_MQTT.cpp ist speicher intensiv mit mehreren Strings.
  // Der recv Aufruf benötigt etwa eine Sekunde
  // Ein if (nanoesp.available()) ist nicht zielführend, es arbeitet nicht zuverlässig.
  mqtt.recvMQTT(0, 40);

  // Sende hin und wieder in Ping, das Ping dauert und stört den MQTT Empfang, daher nicht zu oft.
  if (lc % 64 == 0) {
    Serial.println(F(""));// Zeilenumbruch, dann ist es gut erkennbar.
    if (!mqtt.ping(0)) {
      // Irgendwas ist falsch. Besser mal neu verbinden.
      Serial.println("MQTT Verbindung verloren. Erneuere!");
      mqtt.disconnect(0);
      connect();
    }
  }
  lc++;
  Serial.print(F("."));
}
