#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <VoiceRecognitionV3.h>

#include "CommonModule/MacroHelper.h"
#include "CommonModule/StringHelper.h"
#include "MqttModule/MqttConfig.h"
#include "RadioEncrypted/Helpers.h"

using RadioEncrypted::connectToWifi;
using RadioEncrypted::resetWatchDog;
using RadioEncrypted::connectToMqtt;
using RadioEncrypted::sendLiveData;
using CommonModule::findPosFromEnd;

struct VoiceMqtt
{
    uint8_t index {0};
    const char * topic {nullptr};
    const char * value {nullptr};

    VoiceMqtt(uint8_t index, const char * topic, const char * value): index(index), topic(topic), value(value) {}
};

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, MQTT_CLIENT_NAME

const uint16_t DISPLAY_TIME {60000};
const uint8_t WIFI_RETRY {10};
const uint8_t MQTT_RETRY {6};
const uint8_t PIN_TX {D2};
const uint8_t PIN_RX {D3};
unsigned long lastRefreshTime {0};
const uint32_t BAUD_RATE {9600};
//const uint32_t BAUD_RATE {115200};
const uint16_t RESET_TIME {10000};
const uint8_t MAX_LOADED {7};
const uint8_t MAX_RECOGNIZED_BUFFER {64};

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);
HTTPClient httpClient;
VR voiceRecognition(PIN_TX, PIN_RX);

const VoiceMqtt commands[] {
    {3, "voice1/laistymas/POWER3", "1"},
    {8, "voice1/laistymas/POWER1", "1"},
};

bool loadGroup(VR & voiceRecognition, uint8_t group)
{
    uint8_t index {group * MAX_LOADED};
    uint8_t loadUntil {index + MAX_LOADED};
    if (voiceRecognition.clear() != 0) {
        error("Failed to clear recognizer.");
        return false;
    } 
    info("Load group %d", group);
    while (index < loadUntil) {
        if(voiceRecognition.load(index) != 0) {
            warning("Failed to load record: %d", index);
        }
        index++;
    }
    return true;
}

// signature is limited to 10 chars
bool publishBasedOnSignature(PubSubClient & client, const char * buffer, uint8_t signLength)
{
    uint8_t pos = findPosFromEnd(buffer, signLength, '-');
    if (pos == 0) {
        pos = strlen(buffer) - 1;
    } else {
        pos++;
    }
    char topic[10] {0};
    char message[10] {0};
    strncpy(topic, buffer, MIN(sizeof(topic) - 1, pos));
    strncpy(message, buffer+pos, MIN(sizeof(message) - 1, signLength - pos));

    if (!client.publish(topic, message)) {
        error("Failed to publish state");
        return true;
    } else {
        info("Sent %s %s", topic, message);
        return false;
    }
}

void setup()
{
    voiceRecognition.begin(9600);
    Serial.begin(BAUD_RATE);
    ESP.wdtDisable();
    ESP.wdtEnable(RESET_TIME);

    // We start by connecting to a WiFi network
    WiFi.mode(WIFI_STA);
    wifi.addAP(WLAN_SSID_1, WLAN_PASSWORD_1);
    #ifdef WLAN_SSID_2
    wifi.addAP(WLAN_SSID_2, WLAN_PASSWORD_2);
    #endif

    if (!connectToWifi(wifi, WIFI_RETRY)) {
        error("Unable to connect to wifi. Sleeping..")
        ESP.deepSleep(120e6);
    }
    resetWatchDog();
    delay(500);

    client.setServer(MQTT_SERVER_ADDRESS, 1883);
    if (!connectToMqtt(client, MQTT_CLIENT_NAME, nullptr)) {
        error("Failed to connect to mqtt server on %s", MQTT_SERVER_ADDRESS);
        ESP.deepSleep(120e6);
    }

    resetWatchDog();
    delay(500);

    client.setCallback([](const char * topic, uint8_t * payload, uint16_t len) {
        debug("Mqtt message received for: %s", topic);
    });

    if (!loadGroup(voiceRecognition, 0)) {
        error("VoiceRecognitionModule not found.");
        error("Please check connection and restart Arduino.");
        ESP.deepSleep(120e6);
    }
}

void loop()
{
    client.loop();

    uint8_t buf[MAX_RECOGNIZED_BUFFER] {0};
    uint8_t ret = voiceRecognition.recognize(buf, 50);
    if(ret > 0) {
        bool found {false};
        for (auto command: commands) {
            // buf[1] is train index
            if (command.index == buf[1]) {

                if (!client.publish(command.topic, command.value)) {
                    error("Failed to publish state");
                } else {
                    info("Sent %s %s", command.topic, command.value);
                }

                found = true;
                break;
            }
        }
        if (!found) {
            // buf 3 signature length
            // use sign value e.g. heating-1 or last char heating1, heating0
            if (buf[3] > 0) {
                // buf 4 signature begins
                if (strncmp("group", (char*)buf+4, 5) == 0) {
                    uint8_t group = atoi((char*)buf+9);
                    loadGroup(voiceRecognition, group);
                } else {
                    publishBasedOnSignature(client, (char*)buf+4, MIN(sizeof(buf) - 1, buf[3]));
                }
            } else {
                warning("Record %d has not been mapped", buf[1]);
            }
        }
    }

	if(millis() - lastRefreshTime >= DISPLAY_TIME) {

        if (!connectToWifi(wifi, WIFI_RETRY)) {
            error("Unable to connect to wifi");
        }
        resetWatchDog();
        if (!connectToMqtt(client, MQTT_CLIENT_NAME, nullptr)) {
            error("Failed to connect to mqtt server on %s", MQTT_SERVER_ADDRESS);
        }
        resetWatchDog();

		sendLiveData(client);
        
        info("Ping");
		lastRefreshTime = millis();
	}

    resetWatchDog();
}
