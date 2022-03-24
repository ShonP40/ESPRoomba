#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <Roomba.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>
#include <string.h>
// Your config file
#include "config.h"

//////////////////////////////////////////////////////////////////////////// Config (Modify this in your `config.h` file)
// WiFi
const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// Static routing
#if STATIC_IP
IPAddress local_IP(LOCAL_IP);
IPAddress gateway(GATEWAY);
IPAddress subnet(SUBNET);
IPAddress primaryDNS(PRIMARYDNS);
IPAddress secondaryDNS(SECONDARYDNS);
#endif

// OTA
const char* OTApassword = OTA_PASSWORD;
const int OTAport = OTA_PORT;

// MQTT
const char* mqtt_server = MQTT_SERVER;
const int mqtt_port = MQTT_PORT;
const char* mqtt_user = MQTT_USER;
const char* mqtt_pass = MQTT_PASS;
const char* mqtt_client_name = MQTT_CLIENT_NAME;
//////////////////////////////////////////////////////////////////////////// Prepare the ESP
WiFiClient espClient;
PubSubClient client(espClient);
Roomba roomba(&Serial, Roomba::Baud115200);
//////////////////////////////////////////////////////////////////////////// Variables
bool toggle = true;
const int noSleepPin = 2;
bool boot = true;
uint8_t tempBuf[10];

// MQTT
char mqtt_send_package[50];

// Battery
long battery_Current_mAh = 0;
long charging_state = 0;
long battery_Total_mAh = 0;

long battery_Temperature = 0;

long battery_voltage = 0;

long power_usage = 0;

// Virtual Wall
long virtual_wall = 0;

// Motor Current
long right_motor_current = 0;

long left_motor_current = 0;

long main_brush_motor_current = 0;

long side_brush_motor_current = 0;
//////////////////////////////////////////////////////////////////////////// Time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", UTC_OFFSET);
//////////////////////////////////////////////////////////////////////////// Functions

void setup_wifi() {
    #if STATIC_IP
    WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS);
    #endif
    WiFi.hostname(mqtt_client_name);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
}

void reconnect() {
    // Loop until we're reconnected
    int retries = 0;
    while (!client.connected()) {
        if (retries < 50) {
            // Attempt to connect
            if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass, MQTT_STATUS_TOPIC, 0, 0, "Dead Somewhere")) {
                // Once connected, publish an announcement...
                if (boot == false) {
                    client.publish(MQTT_BOOT_STATUS_TOPIC, "Reconnected");
                }
                if (boot == true) {
                    client.publish(MQTT_BOOT_STATUS_TOPIC, "Rebooted");
                    boot = false;
                }
                // ... and resubscribe
                client.subscribe(MQTT_COMMANDS_TOPIC);
            } else {
                retries++;
                // Wait 5 seconds before retrying
                delay(5000);
            }
        }
        if (retries >= 50) {
            ESP.restart();
        }
    }
}

void callback(char* topic, byte* payload, unsigned int length) {
    String newTopic = topic;
    payload[length] = '\0';
    String newPayload = String((char *)payload);
    if (newTopic == MQTT_COMMANDS_TOPIC) {
        if (newPayload == "start") {
        startCleaning();
        }
        if (newPayload == "stop") {
        stopCleanig();
        }
        if (newPayload == "home") {
        goHome();
        }
        if (newPayload == "clock") {
        setTimeAndDate();
        }
    }
}


void startCleaning() {
    awake();
    Serial.write(128);
    delay(50);
    Serial.write(131);
    delay(50);
    Serial.write(135);
    client.publish(MQTT_STATUS_TOPIC, "Cleaning");
}

void stopCleanig() {
    Serial.write(128);
    delay(50);
    Serial.write(135);
    client.publish(MQTT_STATUS_TOPIC, "Halted");
}

void goHome() {
    awake();
    Serial.write(128);
    delay(50);
    Serial.write(131);
    delay(50);
    Serial.write(143);
    client.publish(MQTT_STATUS_TOPIC, "Returning");
}

void packageAndSendMQTT(String value, char* topic) { // Package up the data and send it to the MQTT server
    value.toCharArray(mqtt_send_package, value.length() + 1);
    client.publish(topic, mqtt_send_package);
}

void setTimeAndDate() {
    #if DEBUG
    // Day
    packageAndSendMQTT(String(timeClient.getDay()), MQTT_DAY_TOPIC);

    // Hour
    packageAndSendMQTT(String(timeClient.getHours()), MQTT_HOUR_TOPIC);
    
    // Minutes
    packageAndSendMQTT(String(timeClient.getMinutes()), MQTT_MINUTES_TOPIC);
    #endif

    awake();
    Serial.write(128);
    delay(50);
    Serial.write(168);
    delay(50);
    Serial.write(timeClient.getDay());
    delay(50);
    Serial.write(timeClient.getHours());
    delay(50);
    Serial.write(timeClient.getMinutes());
}

void sendInfoRoomba() {
    awake();
    roomba.start();

    // Battery status
    roomba.getSensors(21, tempBuf, 1);
    charging_state = tempBuf[0];
    delay(50);

    roomba.getSensors(25, tempBuf, 2);
    battery_Current_mAh = tempBuf[1] + 256 * tempBuf[0];
    delay(50);

    roomba.getSensors(26, tempBuf, 2);
    battery_Total_mAh = tempBuf[1] + 256 * tempBuf[0];

    if (battery_Total_mAh != 0) {
        int nBatPcent = 100 * battery_Current_mAh / battery_Total_mAh;
        packageAndSendMQTT(String(nBatPcent), MQTT_BATTERY_TOPIC);
    } else {
        client.publish(MQTT_BATTERY_TOPIC, "NO DATA");
    }
    
    switch (charging_state) {
      case 0:
        client.publish(MQTT_CHARGING_TOPIC, "Not Charging");
        break;
      case 1:
        client.publish(MQTT_CHARGING_TOPIC, "Reconditioning Charging");
        break;
      case 2:
        client.publish(MQTT_CHARGING_TOPIC, "Fast Charging");
        break;
      case 3:
        client.publish(MQTT_CHARGING_TOPIC, "Trickle Charging");
        break;
      case 4:
        client.publish(MQTT_CHARGING_TOPIC, "Waiting");
        break;
      case 5:
        client.publish(MQTT_CHARGING_TOPIC, "Error");
        break;
    }

    // Battery temperature
    roomba.getSensors(24, tempBuf, 1);
    battery_Temperature = tempBuf[0];

    packageAndSendMQTT(String(battery_Temperature), MQTT_BATTERY_TEMPERATURE_TOPIC);

    // Virtual Wall
    roomba.getSensors(13, tempBuf, 1);
    virtual_wall = tempBuf[0];

    if (virtual_wall == 0) {
      client.publish(MQTT_VIRTUAL_WALL_TOPIC, "Not Detected");
    } else if (virtual_wall == 1) {
      client.publish(MQTT_VIRTUAL_WALL_TOPIC, "Detected");
    }

    // Battery voltage
    roomba.getSensors(22, tempBuf, 2);
    battery_voltage = tempBuf[1] + 256 * tempBuf[0];

    int nBatVolt = battery_voltage / 1000;

    packageAndSendMQTT(String(nBatVolt), MQTT_BATTERY_VOLTAGE_TOPIC);

    // Power usage
    roomba.getSensors(23, tempBuf, 2);
    power_usage = tempBuf[1] + 256 * tempBuf[0];

    int nPwrUsage = power_usage / 1000;

    packageAndSendMQTT(String(nPwrUsage), MQTT_POWER_USAGE_TOPIC);

    // Motor current & Power indicator //

    // Right Motor

    roomba.getSensors(55, tempBuf, 2);
    right_motor_current = tempBuf[1] + 256 * tempBuf[0];

    packageAndSendMQTT(String(right_motor_current), MQTT_RIGHT_MOTOR_CURRENT_TOPIC);

    // Left Motor

    roomba.getSensors(54, tempBuf, 2);
    left_motor_current = tempBuf[1] + 256 * tempBuf[0];

    packageAndSendMQTT(String(left_motor_current), MQTT_LEFT_MOTOR_CURRENT_TOPIC);

    // Main Brush Motor

    roomba.getSensors(56, tempBuf, 2);
    main_brush_motor_current = tempBuf[1] + 256 * tempBuf[0];

    packageAndSendMQTT(String(main_brush_motor_current), MQTT_MAIN_BRUSH_MOTOR_CURRENT_TOPIC);

    // Side Brush Motor

    roomba.getSensors(57, tempBuf, 2);
    side_brush_motor_current = tempBuf[1] + 256 * tempBuf[0];

    packageAndSendMQTT(String(side_brush_motor_current), MQTT_SIDE_BRUH_MOTOR_CURRENT_TOPIC);

    // Power indicator

    if (right_motor_current + left_motor_current + main_brush_motor_current + side_brush_motor_current > 0) {
      client.publish(MQTT_RUNNING_INDICATOR_TOPIC, "Running");
    } else {
      client.publish(MQTT_RUNNING_INDICATOR_TOPIC, "Idle");
    }
}

void awake() {
    digitalWrite(noSleepPin, HIGH);
    delay(1000);
    digitalWrite(noSleepPin, LOW);
    delay(1000);
    digitalWrite(noSleepPin, HIGH);
    delay(1000);
    digitalWrite(noSleepPin, LOW);
}


void setup() {
    // Configure the serial interface
    pinMode(noSleepPin, OUTPUT);
    digitalWrite(noSleepPin, HIGH);
    Serial.begin(115200);
    Serial.write(129);
    delay(50);
    Serial.write(11);
    delay(50);

    // WiFi
    setup_wifi();

    //MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callback);

    // OTA
    ArduinoOTA.setPort(OTAport);
    ArduinoOTA.setHostname(mqtt_client_name);
    ArduinoOTA.setPassword((const char *)OTApassword);
    ArduinoOTA.begin();

    // Time
    timeClient.begin();
}

void loop() {
    // Time
    timeClient.update();

    // OTA Updates
    ArduinoOTA.handle();

    // Roomba info
    sendInfoRoomba();
    delay(1000);

    // MQTT
    if (!client.connected()) {
        reconnect();
    }

    client.loop();
}