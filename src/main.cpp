#include <Arduino.h>
#include <ESP8266mDNS.h>
#include <NTPClient.h>
#include <TimeLib.h>
#include <WiFiManager.h>
#include <WiFiUdp.h>

#include "WebConfig.h"

const int gPin = D6;
const int bPin = D5;

const unsigned long TIME_SYNC_INTERVAL_MS = 60 * 60 * 1000;  // 1 hr
const unsigned long TIME_CHECK_INTERVAL_MS = 60 * 1000;      // 1 minute
const unsigned long WIFI_CHECK_INTERVAL_MS = 5 * 60 * 1000;  // 5 minutes

const String DEVICE_NAME_PREFIX = "wake-up-lights";

WiFiManager wifiManager;
WebConfig webConfig;

// NTP Client
WiFiUDP ntpUDP;
NTPClient ntpClient(ntpUDP, "pool.ntp.org", -8 * 3600,
                    60000);  // Pacific Standard Time (UTC -8)

// Enum for color states
enum ColorState { COLOR_STATE_OFF, COLOR_STATE_BLUE, COLOR_STATE_GREEN };

// Track the last color state
ColorState lastColorState = COLOR_STATE_OFF;

char* getChipId() {
#ifdef ESP32
    uint32_t chipId = 0;
    for (int i = 0; i < 17; i += 8) {
        chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
    }

    // Allocate memory for the hex representation of chipId (8 characters + 1
    // for null terminator)
    static char chipIdStr[9];
    snprintf(chipIdStr, sizeof(chipIdStr), "%08X", chipId);
    return chipIdStr;
#elif defined(ESP8266)
    // Get chip ID for ESP8266
    uint32_t chipId = ESP.getChipId();

    // Allocate memory for the hex representation of chipId (6 characters + 1
    // for null terminator)
    static char chipIdStr[7];
    snprintf(chipIdStr, sizeof(chipIdStr), "%06X", chipId);
    return chipIdStr;
#endif
}

void reset() {
    digitalWrite(gPin, LOW);
    digitalWrite(bPin, LOW);
}

void setColor(int g, int b) {
    // reset();
    analogWrite(gPin, g);
    analogWrite(bPin, b);
}

// Function to update time from NTP
void updateTimeFromNTP() {
    if (ntpClient.update()) {
        unsigned long epochTime = ntpClient.getEpochTime();
        setTime(epochTime);  // Set Time library's time

        Serial.print("Current time: ");
        Serial.print(hour());
        Serial.print(":");
        Serial.print(minute());
        Serial.print(":");
        Serial.println(second());
    } else {
        Serial.println("Failed to get time from NTP");
    }
}

// Function to check if current time is within the start and end time
bool isTimeInRange(int startHour, int startMin, int endHour, int endMin,
                   int currentHour, int currentMinute) {
    // Convert all times to minutes from start of day for easy comparison
    int startTotalMinutes = startHour * 60 + startMin;
    int endTotalMinutes = endHour * 60 + endMin;
    int currentTotalMinutes = currentHour * 60 + currentMinute;

    // Check if current time is within the range (handle wrapping of time at
    // midnight)
    if (startTotalMinutes <= endTotalMinutes) {
        // No wrap around (same day)
        return currentTotalMinutes >= startTotalMinutes &&
               currentTotalMinutes <= endTotalMinutes;
    } else {
        // Wrap around midnight
        return currentTotalMinutes >= startTotalMinutes ||
               currentTotalMinutes <= endTotalMinutes;
    }
}

void setup() {
    const static String DEVICE_NAME =
        DEVICE_NAME_PREFIX + "-" + String(getChipId());

    pinMode(gPin, OUTPUT);
    pinMode(bPin, OUTPUT);

    Serial.begin(9600);
    while (!Serial);
    delay(200);

    Serial.println("Starting");

    wifiManager.autoConnect(DEVICE_NAME.c_str());
    // randomSeed(analogRead(A0));

    // Initialize NTP and set system time
    ntpClient.begin();
    updateTimeFromNTP();

    // Initialize mDNS
    if (MDNS.begin(DEVICE_NAME)) {
        Serial.println("mDNS responder started at " + DEVICE_NAME + ".local");
    }

    webConfig.addField("deviceName", "string", DEVICE_NAME,
                       "Enter the device name");
    webConfig.addField("blueStartHour", "int", "20",
                       "Hour to turn on blue light (24-hour format)");
    webConfig.addField("blueStartMin", "int", "30",
                       "Minute to turn on blue light");
    webConfig.addField("blueEndHour", "int", "7",
                       "Hour to turn off blue light (24-hour format)");
    webConfig.addField("blueEndMin", "int", "30",
                       "Minute to turn off blue light");

    webConfig.addField("greenStartHour", "int", "7",
                       "Hour to turn on green light (24-hour format)");
    webConfig.addField("greenStartMin", "int", "30",
                       "Minute to turn on green light");
    webConfig.addField("greenEndHour", "int", "9",
                       "Hour to turn off green light (24-hour format)");
    webConfig.addField("greenEndMin", "int", "0",
                       "Minute to turn off green light");

    webConfig.begin();
}

void loop() {
    webConfig.handleClient();

    unsigned long currentMillis = millis();
    static unsigned long lastWifiCheckMillis = 0;

    if ((currentMillis - lastWifiCheckMillis >= WIFI_CHECK_INTERVAL_MS) ||
        (lastWifiCheckMillis == 0)) {
        lastWifiCheckMillis = currentMillis;  // Update the last check time

        // Check WiFi connection status
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("WiFi disconnected! Attempting to reconnect...");

            // Attempt to reconnect
            if (WiFi.reconnect()) {
                Serial.println("Reconnected to WiFi!");
                Serial.print("IP Address: ");
                Serial.println(WiFi.localIP());
            } else {
                Serial.println(
                    "Reconnection failed. Will try again next check.");
            }
        }
    }

    // Update time from NTP every hour
    static unsigned long lastNtpSyncMillis = 0;
    if (currentMillis - lastNtpSyncMillis >
        TIME_SYNC_INTERVAL_MS) {  // Sync every hour
        updateTimeFromNTP();
        lastNtpSyncMillis = currentMillis;
    }

    static unsigned long lastColorCheckMillis = 0;
    if ((currentMillis - lastColorCheckMillis > TIME_CHECK_INTERVAL_MS) ||
        (lastColorCheckMillis == 0)) {
        lastColorCheckMillis = currentMillis;

        // Check time for LED color change
        int currentHour = hour();
        int currentMinute = minute();

        // Determine the desired color based on the time
        ColorState currentColor = COLOR_STATE_OFF;

        int blueStartHour = webConfig.getValue("blueStartHour").toInt();
        int blueStartMin = webConfig.getValue("blueStartMin").toInt();
        int blueEndHour = webConfig.getValue("blueEndHour").toInt();
        int blueEndMin = webConfig.getValue("blueEndMin").toInt();

        int greenStartHour = webConfig.getValue("greenStartHour").toInt();
        int greenStartMin = webConfig.getValue("greenStartMin").toInt();
        int greenEndHour = webConfig.getValue("greenEndHour").toInt();
        int greenEndMin = webConfig.getValue("greenEndMin").toInt();

        Serial.println();
        Serial.print("currentHour = ");
        Serial.println(currentHour);
        Serial.print("currentMinute = ");
        Serial.println(currentMinute);
        Serial.println();
        Serial.print("blueStartHour = ");
        Serial.println(blueStartHour);
        Serial.print("blueStartMin = ");
        Serial.println(blueStartMin);
        Serial.print("blueEndHour = ");
        Serial.println(blueEndHour);
        Serial.print("blueEndMin = ");
        Serial.println(blueEndMin);

        Serial.print("greenStartHour = ");
        Serial.println(greenStartHour);
        Serial.print("greenStartMin = ");
        Serial.println(greenStartMin);
        Serial.print("greenEndHour = ");
        Serial.println(greenEndHour);
        Serial.print("greenStartHour = ");
        Serial.println(greenEndMin);
        Serial.println();
        Serial.println();

        if (isTimeInRange(blueStartHour, blueStartMin, blueEndHour, blueEndMin,
                          currentHour, currentMinute)) {
            currentColor = COLOR_STATE_BLUE;
        } else if (isTimeInRange(greenStartHour, greenStartMin, greenEndHour,
                                 greenEndMin, currentHour, currentMinute)) {
            currentColor = COLOR_STATE_GREEN;
        } else {
            currentColor = COLOR_STATE_OFF;
        }

        // Check if the color has changed
        if (currentColor != lastColorState) {
            // Update the LED color and print it to Serial
            switch (currentColor) {
                case COLOR_STATE_BLUE:
                    setColor(0, 255);  // Blue
                    Serial.println("Color changed to: BLUE");
                    break;
                case COLOR_STATE_GREEN:
                    setColor(255, 0);  // Green
                    Serial.println("Color changed to: GREEN");
                    break;
                case COLOR_STATE_OFF:
                    setColor(0, 0);  // Off
                    Serial.println("Color changed to: OFF");
                    break;
            }

            // Update the last color state
            lastColorState = currentColor;
        }
    }
}
