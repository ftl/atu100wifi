/*
  This piece of software receives the display content of the ATU-100 on the serial interface
  and publishes the measurement data via MQTT.

  The following topics are used to publish data:
  * MQTT_DATA_TOPIC: all the measurement data as JSON object, on every change
  * MQTT_RSSI_TOPIC: the current RSSI of the wifi, every 10 seconds
  * MQTT_WILL_TOPIC: "true" as soon as the MQTT connection is established, "false" as last will

  The software also subscribes to MQTT_CMD_TOPIC in order to receive commands:
  * "1" will push the button for 500ms
  
  All configuration data is defined in the file include/config.h. Create it from include/config-template.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.h" // create this header from include/config-template.h and fill it with all your configuration data

#define BUTTON D1

void onMQTTMessage(char*, byte*, unsigned int);

WiFiClient client;
WiFiClient wifi;
PubSubClient mqtt(wifi);
bool wifiConnected = false;
bool mqttConnected = false;

long lastBeat = 0L;

String displayContent;
uint8_t currentIndex = 0;

void handleDisplayUpdate();

void blinkLED(int on, int off) {
  digitalWrite(LED_BUILTIN, LOW);
  delay(on);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(off);
}

void publishString(String topic, String value) {
  char topicBuf[topic.length() + 1];
  topic.toCharArray(topicBuf, topic.length() + 1);

  char valueBuf[value.length() + 1];
  value.toCharArray(valueBuf, value.length() + 1);
    
  mqtt.publish(topicBuf, valueBuf, true);  
}


void heartbeat(int seconds) {
  if (!mqtt.connected()) return;

  long now = millis();
  if (now - lastBeat < seconds * 1000) return;
  lastBeat = now;

  String rssi = String(WiFi.RSSI());

  publishString(MQTT_RSSI_TOPIC, rssi);
}
void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(BUTTON, OUTPUT);
  digitalWrite(BUTTON, LOW);
  Serial.begin(9600);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No WiFi connection");
    wifiConnected = false;
    WiFi.hostname(WIFI_HOSTNAME);
    WiFi.begin(WIFI_SSID, WIFI_PASSPHRASE);
    for (int i = 0; i < 10 && WiFi.status() != WL_CONNECTED; i++) {
      blinkLED(50, 450);
    }

    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("WiFi connected");
      wifiConnected = true;
      mqtt.setServer(MQTT_HOST, MQTT_PORT);
    }
    return;
  }

  if (!mqttConnected) {
    Serial.println("No MQTT connection");
    blinkLED(50, 50);
    blinkLED(50, 350);
    mqtt.setCallback(onMQTTMessage);
    mqttConnected = mqtt.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD, MQTT_WILL_TOPIC, 2, true, "false");
    if (mqttConnected) {
      Serial.println("MQTT connected");
      publishString(MQTT_WILL_TOPIC, "true");
      mqtt.subscribe(MQTT_CMD_TOPIC);
    }
    return;
  }

  digitalWrite(LED_BUILTIN, LOW);

  // handle the input from the Arduino Nano
  if (Serial.available()) {
      char c = Serial.read();
      if (c == 0x0A) {
          handleDisplayUpdate();
          currentIndex = 0;
          displayContent = "";
      } else if (c < 0x20) {
          // ignore control characters
      } else {
          displayContent += c;
      }
  }

  heartbeat(10);

  mqtt.loop();
}

void handleDisplayUpdate() {
  if (displayContent.substring(0, 4) != "PWR=") {
    Serial.println("wrong line format");
    return;
  }

  String pwrIn = displayContent.substring(4, 7);
  pwrIn.trim();
  bool autoMode = (displayContent.charAt(8) == '.');
  bool txing = (displayContent.charAt(11) == '=') || ((displayContent.charAt(76) == '='));
  bool tuning = displayContent.substring(68, 72) == "TUNE";
  bool resetting = displayContent.substring(64, 69) == "RESET";

  String swr = "";
  if (displayContent.substring(64, 68) == "SWR=" && !tuning) {
    swr = displayContent.substring(68, 72);
  }

  String pwrOut = "";
  String efficiency = "";
  bool lcNetwork = false;
  String inductance = "";
  String capacitance = "";
  if (txing) {
    pwrOut = displayContent.substring(12, 15);
    efficiency = displayContent.substring(77, 79);
  } else if (displayContent.charAt(9) == 'L') {
    lcNetwork = true;
    inductance = displayContent.substring(11, 15);
    capacitance = displayContent.substring(75, 79);
    capacitance.trim();
  } else {
    lcNetwork = false;
    inductance = displayContent.substring(75, 79);
    capacitance = displayContent.substring(11, 15);
    capacitance.trim();
  }

  String json = "{";
  if (tuning) {
    json += "\"tuning\":true,";
  } else {
    json += "\"tuning\":false,";
  }
  if (resetting) {
    json += "\"resetting\":true,";
  } else {
    json += "\"resetting\":false,";
  }
  if (autoMode) {
    json += "\"auto_mode\":true,";
  } else {
    json += "\"auto_mode\":false,";
  }
  json += "\"pwr_in\":" + pwrIn + ",";
  if (txing) {
    json += "\"txing\":true,";
    json += "\"pwr_out\":" + pwrOut + ",";
    json += "\"efficiency\":" + efficiency + ",";
  } else {
    json += "\"txing\":false,";
    json += "\"pwr_out\":0,";
    json += "\"efficiency\":0,";
  }
  if (swr != "") {
    json += "\"swr\":" + swr + ",";
  } else {
    json += "\"swr\":0,";
  }
  if (lcNetwork) {
    json += "\"lc_network\":true";
  } else {
    json += "\"lc_network\":false";
  }
  if (inductance != "" && capacitance != "") {
    json += ",\"inductance\":" + inductance + ",";
    json += "\"capacitance\":" + capacitance;
  }
  json += "}";

  publishString(MQTT_DATA_TOPIC, json);
}

void pressButton() {
  digitalWrite(BUTTON, HIGH);
  delay(500);
  digitalWrite(BUTTON, LOW);
}

void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  if (length == 0) {
    return;
  }
  char c = (char)payload[0];
  if (c == '1') {
    pressButton();
  }
}