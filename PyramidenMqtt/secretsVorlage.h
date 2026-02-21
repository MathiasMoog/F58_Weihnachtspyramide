// Hier die Zugangsdaten für das WiFi eintragen
#define SECRET_SSID F("xxx")
#define SECRET_PASS F("xxx")
// Hier die Zugangsdaten zum MQTT Server eintragen
#define MQTT_IP F("xxx")
#define MQTT_PORT 1883
#define MQTT_ID F("WP1")
// Falls kein Passwortschutz vorhanden ist auch aus dem Code herausnehmen, NanoESP Bib. Anleitung lesen
#define MQTT_USER F("xxx")
#define MQTT_PASS F("xxx")
// Die MQTT topics
// Achtung, keine zu langen topics nehmen, der Speicher ist sehr knapp
#define TOPIC_MOTOR_STATE  F("openhab/pyramide/motor/state")
#define TOPIC_MOTOR_SET  F("openhab/pyramide/motor/set")
#define TOPIC_KERZEN_STATE  F("openhab/pyramide/kerzen/state")
#define TOPIC_KERZEN_SET  F("openhab/pyramide/kerzen/set")
