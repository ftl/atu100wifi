/*
    This header contains all the configuration data.
*/

#define WIFI_SSID "myssid"
#define WIFI_PASSPHRASE "mysecretpassphrase"
#define WIFI_HOSTNAME "myatu100"
// uncomment to use the built-in LED to indicate the WIFI status
// ATTENTION: this collides with the support for the BYPASS button. 
// If you use WIFI_LED, do NOT connect the BYPASS button!
// #define WIFI_LED

#define MQTT_HOST "192.168.1.1"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID "atu100"
#define MQTT_USER ""
#define MQTT_PASSWORD ""
#define MQTT_DATA_TOPIC "hamradio/atu100/data"
#define MQTT_RSSI_TOPIC "hamradio/atu100/rssi"
#define MQTT_WILL_TOPIC "hamradio/atu100/alive"
#define MQTT_CMD_TOPIC "hamradio/atu100/cmd"
#define MQTT_TRAFO_RELAY_TOPIC "afu/atu100/relays/trafo"
#define MQTT_ANT1_RELAY_TOPIC "afu/atu100/relays/ant1"
#define MQTT_ANT2_RELAY_TOPIC "afu/atu100/relays/ant2"
#define MQTT_ANT3_RELAY_TOPIC "afu/atu100/relays/ant3"
