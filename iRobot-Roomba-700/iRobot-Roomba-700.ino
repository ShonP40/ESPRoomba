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
//////////////////////////////////////////////////////////////////////////// Prepare the ESP
WiFiClient espClient;
PubSubClient client(espClient);
Roomba roomba(&Serial, Roomba::Baud115200);
//////////////////////////////////////////////////////////////////////////// Variables
const int noSleepPin = 2;
uint8_t tempBuf[10];

#if DEBUG
bool boot = true;
#endif

// MQTT
char mqtt_send_package[50];

// Battery
int nBatPcent;
long battery_Current_mAh = 0;
long charging_state = 0;
long battery_Total_mAh = 0;

#if SENSORS
long battery_Temperature = 0;

long battery_voltage = 0;

long power_usage = 0;

// Virtual Wall
long virtual_wall = 0;
#endif

// Motor Current
long right_motor_current = 0;

long left_motor_current = 0;

long main_brush_motor_current = 0;

long side_brush_motor_current = 0;

// Running indicator
bool roomba_running;
bool roomba_cleaning;
bool roomba_returning;
bool roomba_halted;
bool charging;
bool roomba_spot_cleaning;
bool roomba_max_cleaning;

// Charging Sources Available
long charging_sources_available = 0;

// Roomba buttons
String roomba_buttons;
//////////////////////////////////////////////////////////////////////////// Functions

// Configure the WiFi interface
void setup_wifi() {
    #if STATIC_IP
    WiFi.config(IPAddress(LOCAL_IP), IPAddress(GATEWAY), IPAddress(SUBNET), IPAddress(PRIMARYDNS), IPAddress(SECONDARYDNS));
    #endif
    WiFi.hostname(MQTT_CLIENT_NAME);
    #if USE_BSSID
    uint8_t bssid[6];
    char* ptr;
    bssid[0] = strtol(WIFI_BSSID, &ptr, HEX);
    for (uint8_t i = 1; i < 6; i++) {
        bssid[i] = strtol(ptr+1, &ptr, HEX);
    }
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD, 0, bssid, true);
    #else
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    #endif
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
}

// Reconnect to WiFi & MQTT
void reconnect() {
    // Loop until we're reconnected
    int retries = 0;
    while (!client.connected()) {
        if (retries < 50) {
            // Attempt to connect
            if (client.connect(MQTT_CLIENT_NAME, MQTT_USER, MQTT_PASS, MQTT_STATUS_TOPIC, 0, 0, "Dead Somewhere")) {
                #if DEBUG
                // Once connected, publish an announcement...
                if (boot == false) {
                    client.publish(MQTT_BOOT_STATUS_TOPIC, "Reconnected");
                }
                if (boot == true) {
                    client.publish(MQTT_BOOT_STATUS_TOPIC, "Rebooted");
                    boot = false;
                }
                #endif
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

// Receive MQTT commands
void callback(char* topic, byte* payload, unsigned int length) {
    String newTopic = topic;
    payload[length] = '\0';
    String newPayload = String((char *)payload);
    if (newTopic == MQTT_COMMANDS_TOPIC) {
        if (newPayload == "start") {
            startCleaning();
        }
        if (newPayload == "stop") {
            stopCleaning();
        }
        if (newPayload == "return_home") {
            returnToDock();
        }
        if (newPayload == "clock") {
            setTimeAndDate();
        }
        if (newPayload == "restartESP") {
            ESP.restart();
        }
        if (newPayload == "restartRoomba") {
            restartRoomba();
        }
        if (newPayload == "spot") {
            startSpotCleaning();
        }
        if (newPayload == "max") {
            startMaxCleaning();
        }
        if (newPayload == "powerOffRoomba") {
            powerOffRoomba();
        }
    }
}

// Wake up the Roomba from normal sleep
void awake() {
    digitalWrite(noSleepPin, HIGH);
    delay(1000);
    digitalWrite(noSleepPin, LOW);
    delay(1000);
    digitalWrite(noSleepPin, HIGH);
    delay(1000);
    digitalWrite(noSleepPin, LOW);
}

// Package up the provided data and send it to the MQTT broker
void packageAndSendMQTT(String value, char* topic) {
    value.toCharArray(mqtt_send_package, value.length() + 1);
    client.publish(topic, mqtt_send_package);
}

void sendInfoRoomba() {
    awake();
    roomba.start();

    // Battery status
    roomba.getSensors(21, tempBuf, 1);
    charging_state = tempBuf[0];
    delay(50);

    // Get the current battery capacity
    roomba.getSensors(25, tempBuf, 2);
    battery_Current_mAh = tempBuf[1] + 256 * tempBuf[0];
    delay(50);

    // Get the maximum battery capacity
    roomba.getSensors(26, tempBuf, 2);
    battery_Total_mAh = tempBuf[1] + 256 * tempBuf[0];

    // Calculate battery percentage
    if (battery_Total_mAh != 0) {
        nBatPcent = 100 * battery_Current_mAh / battery_Total_mAh;
        packageAndSendMQTT(String(nBatPcent), MQTT_BATTERY_TOPIC);
    }
    
    # if DEBUG
    // Get and report the advanced charging status
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
    #endif

    // Charging Sources Available
    roomba.getSensors(34, tempBuf, 1);
    charging_sources_available = tempBuf[0];
    #if SENSORS
    if (charging_sources_available > 0) {
        packageAndSendMQTT("Dock", MQTT_CHARGING_SOURCES_AVAILABLE_TOPIC);
    } else {
        packageAndSendMQTT("None", MQTT_CHARGING_SOURCES_AVAILABLE_TOPIC);
    }
    #endif

    // Fetch/Guess and report the charging state
    if (charging_state == 1 || charging_state == 2 || charging_state == 3) {
        client.publish(MQTT_STATUS_TOPIC, "Charging");
        charging = true;

        roomba_cleaning = false;
        roomba_returning = false;
        roomba_halted = false;
        roomba_spot_cleaning = false;
        roomba_max_cleaning = false;
    } else if (charging_sources_available > 0) {
        client.publish(MQTT_STATUS_TOPIC, "Charging");
        charging = true;
    } else {
        charging = false;
    }

    #if SENSORS
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
    #endif

    // Motor current & Power indicator //

    // Right Motor
    roomba.getSensors(55, tempBuf, 2);
    right_motor_current = tempBuf[1] + 256 * tempBuf[0];
    #if SENSORS
    packageAndSendMQTT(String(right_motor_current), MQTT_RIGHT_MOTOR_CURRENT_TOPIC);
    #endif

    // Left Motor
    roomba.getSensors(54, tempBuf, 2);
    left_motor_current = tempBuf[1] + 256 * tempBuf[0];
    #if SENSORS
    packageAndSendMQTT(String(left_motor_current), MQTT_LEFT_MOTOR_CURRENT_TOPIC);
    #endif

    // Main Brush Motor
    roomba.getSensors(56, tempBuf, 2);
    main_brush_motor_current = tempBuf[1] + 256 * tempBuf[0];
    #if SENSORS
    packageAndSendMQTT(String(main_brush_motor_current), MQTT_MAIN_BRUSH_MOTOR_CURRENT_TOPIC);
    #endif

    // Side Brush Motor
    roomba.getSensors(57, tempBuf, 2);
    side_brush_motor_current = tempBuf[1] + 256 * tempBuf[0];
    #if SENSORS
    packageAndSendMQTT(String(side_brush_motor_current), MQTT_SIDE_BRUSH_MOTOR_CURRENT_TOPIC);
    #endif

    // Running indicator
    if ((right_motor_current + left_motor_current + main_brush_motor_current + side_brush_motor_current > 0) && !charging) {
        #if DEBUG
        client.publish(MQTT_RUNNING_INDICATOR_TOPIC, "Running");
        #endif
        roomba_running = true;
    } else {
        #if DEBUG
        client.publish(MQTT_RUNNING_INDICATOR_TOPIC, "Idle");
        #endif
        roomba_running = false;
    }

    // Buttons
    roomba.getSensors(18, tempBuf, 1);
    switch (tempBuf[0]) {
        case 0:
            roomba_buttons = "None";
            break;
        case 1:
            roomba_buttons = "Clean";
            roombaCommandedStatus(1);
            break;
        case 2:
            roomba_buttons = "Spot";
            roombaCommandedStatus(4);
            break;
        case 4:
            roomba_buttons = "Dock";
            roombaCommandedStatus(2);
            break;
    }
    #if SENSORS
    packageAndSendMQTT(roomba_buttons, MQTT_ROOMBA_BUTTONS_TOPIC);
    #endif
}

// Set the commanded state and report to MQTT
void roombaCommandedStatus(int status) {
    switch (status) {
        case 1:
            roomba_cleaning = true;
            roomba_returning = false;
            roomba_halted = false;
            roomba_spot_cleaning = false;
            roomba_max_cleaning = false;
            roomba_running = true;
            client.publish(MQTT_STATUS_TOPIC, "Cleaning");
            break;
        case 2:
            roomba_cleaning = false;
            roomba_returning = true;
            roomba_halted = false;
            roomba_spot_cleaning = false;
            roomba_max_cleaning = false;
            roomba_running = true;
            client.publish(MQTT_STATUS_TOPIC, "Returning");
            break;
        case 3:
            roomba_cleaning = false;
            roomba_returning = false;
            roomba_halted = true;
            roomba_spot_cleaning = false;
            roomba_max_cleaning = false;
            roomba_running = false;
            client.publish(MQTT_STATUS_TOPIC, "Halted");
            break;
        case 4:
            roomba_cleaning = false;
            roomba_returning = false;
            roomba_halted = false;
            roomba_spot_cleaning = true;
            roomba_max_cleaning = false;
            roomba_running = true;
            client.publish(MQTT_STATUS_TOPIC, "Spot Cleaning");
            break;
        case 5:
            roomba_cleaning = false;
            roomba_returning = false;
            roomba_halted = false;
            roomba_spot_cleaning = false;
            roomba_max_cleaning = true;
            roomba_running = true;
            client.publish(MQTT_STATUS_TOPIC, "Max Cleaning");
            break;
    }
}

// Guess the Roomba's current status
void roombaStatus() {
    if (roomba_running && !roomba_cleaning && !roomba_returning && !roomba_spot_cleaning && !roomba_max_cleaning) {
        roombaCommandedStatus(1);
    } else if (roomba_running && roomba_cleaning) {
        roombaCommandedStatus(1);
    } else if (roomba_running && roomba_returning) {
        roombaCommandedStatus(2);
    } else if (!roomba_running && !charging) {
        roombaCommandedStatus(3);
    } else if (roomba_running && roomba_spot_cleaning) {
        roombaCommandedStatus(4);
    } else if (roomba_running && roomba_max_cleaning) {
        roombaCommandedStatus(5);
    }
}

// Wake up from deep sleep (when docked)
void awakeFromDeepSleep() {
    if (charging) {
        Serial.write(128);
        delay(50);
        Serial.write(135);
        delay(50);
    }
}

// Stop the Roomba
void stopCleaning() {
    Serial.write(128);
    delay(50);
    Serial.write(131);
    delay(50);
    Serial.write(173);
    delay(50);
    roombaCommandedStatus(3);
}

// These actions are being used for most of the Roomba's commands
void commonCommandActions() {
    awake();
    awakeFromDeepSleep();
    delay(50);
    stopCleaning();
    delay(50);
    Serial.write(128);
    delay(50);
    Serial.write(131);
    delay(50);
}

// Start cleaning
void startCleaning() {
    commonCommandActions();
    Serial.write(135);
    delay(50);
    roombaCommandedStatus(1);
}

// Return to the dock
void returnToDock() {
    commonCommandActions();
    Serial.write(143);
    delay(50);
    roombaCommandedStatus(2);
}

// Start spot cleaning
void startSpotCleaning() {
    commonCommandActions();
    Serial.write(134);
    delay(50);
    roombaCommandedStatus(4);
}

// Start max cleaning
void startMaxCleaning() {
    commonCommandActions();
    Serial.write(136);
    delay(50);
    roombaCommandedStatus(5);
}

// Set the Roomba's clock
void setTimeAndDate() {
    int utc_offset;
    #if DST
    utc_offset = UTC_OFFSET + 3600;
    #else
    utc_offset = UTC_OFFSET;
    #endif

    WiFiUDP ntpUDP;
    NTPClient timeClient(ntpUDP, "pool.ntp.org", utc_offset);

    timeClient.begin();
    delay(1000);
    timeClient.update();
    delay(1000);

    awake();
    awakeFromDeepSleep();
    delay(50);
    Serial.write(128);
    delay(50);
    Serial.write(168);
    delay(50);
    Serial.write(timeClient.getDay());
    delay(50);
    Serial.write(timeClient.getHours());
    delay(50);
    Serial.write(timeClient.getMinutes());

    #if DEBUG
    // Day
    packageAndSendMQTT(String(timeClient.getDay()), MQTT_DAY_TOPIC);

    // Hour
    packageAndSendMQTT(String(timeClient.getHours()), MQTT_HOUR_TOPIC);
    
    // Minutes
    packageAndSendMQTT(String(timeClient.getMinutes()), MQTT_MINUTES_TOPIC);
    #endif
}

// Restart the Roomba (same as removing the battery)
void restartRoomba() {
    commonCommandActions();
    Serial.write(131);
    delay(50);
    Serial.write(7);
}

// Power off the Roomba
void powerOffRoomba() {
    commonCommandActions();
    delay(50);
    Serial.write(133);
}

//////////////////////////////////////////////////////////////////////////// Main

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
    client.setServer(MQTT_SERVER, MQTT_PORT);
    client.setCallback(callback);

    // OTA
    ArduinoOTA.setPort(OTA_PORT);
    ArduinoOTA.setHostname(MQTT_CLIENT_NAME);
    ArduinoOTA.setPassword((const char *)OTA_PASSWORD);
    ArduinoOTA.begin();

    // Time
    setTimeAndDate();
}

void loop() {
    // OTA Updates
    ArduinoOTA.handle();

    // Roomba info
    sendInfoRoomba();
    roombaStatus();
    delay(1000);

    // MQTT
    if (!client.connected()) {
        reconnect();
    }

    client.loop();
}