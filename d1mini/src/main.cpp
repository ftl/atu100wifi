/*
  This piece of software receives the display content of the ATU-100 on the serial interface
  and publishes the measurement data via MQTT.

  The following topics are used to publish data:
  * MQTT_DATA_TOPIC: all the measurement data as JSON object, on every change
  * MQTT_RSSI_TOPIC: the current RSSI of the wifi, every 10 seconds
  * MQTT_WILL_TOPIC: "true" as soon as the MQTT connection is established, "false" as last will

  The software also subscribes to MQTT_CMD_TOPIC in order to receive commands:
  * "1" will push the TUNE button for 500ms
  * "2" will push the AUTO button for 500ms
  * "3" will push the BYPASS button for 500ms
  * "4" will turn on the TRAFO relay
  * "5" will turn off the TRAFO relay
  * "6" will turn on the ANT1 relay
  * "7" will turn on the ANT2 relay
  * "8" will turn on the ANT3 relay
  
  All configuration data is defined in the file include/config.h. Create it from include/config-template.
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <PubSubClient.h>
#include "config.h" // create this header from include/config-template.h and fill it with all your configuration data

#define TUNE_BTN D0
#define AUTO_BTN D3
#define BYPASS_BTN D4 // this is the LED_BUILTIN and we need it to indicate the status of the WIFI connection

#define TRAFO_RLY D5
#define ANT1_RLY D6
#define ANT2_RLY D7
#define ANT3_RLY D8

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
  #ifdef WIFI_LED
  digitalWrite(LED_BUILTIN, LOW);
  delay(on);
  digitalWrite(LED_BUILTIN, HIGH);
  delay(off);
  #endif
}

void publishString(String topic, String value) {
  char topicBuf[topic.length() + 1];
  topic.toCharArray(topicBuf, topic.length() + 1);

  char valueBuf[value.length() + 1];
  value.toCharArray(valueBuf, value.length() + 1);
    
  mqtt.publish(topicBuf, valueBuf, true);  
}

void publishInt(String topic, int value) {
  String str = String(value);
  publishString(topic, str);
}

void heartbeat(int seconds) {
  if (!mqtt.connected()) return;

  long now = millis();
  if (now - lastBeat < seconds * 1000) return;
  lastBeat = now;

  String rssi = String(WiFi.RSSI());

  publishString(MQTT_RSSI_TOPIC, rssi);
}

void setupOutput(uint8_t pin) {
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  setupOutput(TUNE_BTN);
  setupOutput(AUTO_BTN);
  setupOutput(TRAFO_RLY);
  setupOutput(ANT1_RLY);
  setupOutput(ANT2_RLY);
  setupOutput(ANT3_RLY);

  digitalWrite(ANT1_RLY, HIGH);

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

  #ifdef WIFI_LED
  digitalWrite(LED_BUILTIN, LOW);
  #endif

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

void pressButton(uint8_t pin) {
  Serial.print("press button ");
  Serial.println(pin);
  digitalWrite(pin, HIGH);
  delay(500);
  digitalWrite(pin, LOW);
}

void switchTrafo(uint8_t val) {
  Serial.print("switch trafo ");
  Serial.println(val);
  digitalWrite(TRAFO_RLY, val);
  publishInt(MQTT_TRAFO_RELAY_TOPIC, val);
}

void selectAntenna(uint8_t antenna) {
  Serial.print("select antenna ");
  Serial.println(antenna);
  switch (antenna) {
  case 1:
    digitalWrite(ANT1_RLY, HIGH);
    digitalWrite(ANT2_RLY, LOW);
    digitalWrite(ANT3_RLY, LOW);
    break;
  case 2:
    digitalWrite(ANT1_RLY, LOW);
    digitalWrite(ANT2_RLY, HIGH);
    digitalWrite(ANT3_RLY, LOW);
    break;
  case 3:
    digitalWrite(ANT1_RLY, LOW);
    digitalWrite(ANT2_RLY, LOW);
    digitalWrite(ANT3_RLY, HIGH);
    break;
  default:
    antenna = 1;
    digitalWrite(ANT1_RLY, HIGH);
    digitalWrite(ANT2_RLY, LOW);
    digitalWrite(ANT3_RLY, LOW);
  }
  publishInt(MQTT_ANT1_RELAY_TOPIC, (antenna == 1));
  publishInt(MQTT_ANT2_RELAY_TOPIC, (antenna == 2));
  publishInt(MQTT_ANT3_RELAY_TOPIC, (antenna == 3));
}

void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  if (length == 0) {
    return;
  }
  char c = (char)payload[0];
  switch (c) {
  case '1':
    pressButton(TUNE_BTN);
    break;
  case '2':
    pressButton(TUNE_BTN);
    break;
  case '3':
    pressButton(TUNE_BTN);
    break;
  case '4':
    switchTrafo(HIGH);
    break;
  case '5':
    switchTrafo(LOW);
    break;
  case '6':
    selectAntenna(1);
    break;
  case '7':
    selectAntenna(2);
    break;
  case '8':
    selectAntenna(3);
    break;
  }
}