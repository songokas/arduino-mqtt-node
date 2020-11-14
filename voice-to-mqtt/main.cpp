#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <VoiceRecognitionV3.h>
#include <EEPROM.h>

#include "CommonModule/MacroHelper.h"
#include "CommonModule/StringHelper.h"
#include "MqttModule/MqttConfig.h"
#include "RadioEncrypted/Helpers.h"

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, MQTT_CLIENT_NAME

using RadioEncrypted::connectToWifi;
using RadioEncrypted::resetWatchDog;
using RadioEncrypted::connectToMqtt;
using RadioEncrypted::sendLiveData;
using CommonModule::findPosFromEnd;
using CommonModule::findNextPos;

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
const char CHANNEL_TRAIN[] PROGMEM {"voice/train/#"};
const uint8_t MAX_SIGNATURE_LENGTH {10};
const uint8_t EEPROM_CACHE {200};
const uint8_t MAX_COMMANDS {20};
const char DEFAULT_MESSAGE[] {"toggle"};

#include "helpers.h"

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);
HTTPClient httpClient;
VR voiceRecognition(PIN_TX, PIN_RX);
VoiceMqtt commands[MAX_COMMANDS] {};

void setup()
{
    voiceRecognition.begin(9600);
    Serial.begin(BAUD_RATE);
    EEPROM.begin(EEPROM_CACHE);
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
    if (!connectToMqtt(client, MQTT_CLIENT_NAME, CHANNEL_TRAIN)) {
        error("Failed to connect to mqtt server on %s", MQTT_SERVER_ADDRESS);
        ESP.deepSleep(120e6);
    }

    resetWatchDog();
    delay(500);


    if (EEPROM.read(EEPROM_CACHE) != 255) {
        EEPROM.get(EEPROM_CACHE, commands);
    } else {
        commands[3] = VoiceMqtt::fromFlashString(PSTR("voice1/laistymas/POWER1"), PSTR("toggle"));
        commands[8] = VoiceMqtt::fromFlashString(PSTR("voice1/laistymas/POWER3"), PSTR("1"));
    }

    //expects topic -> message : voice/train/window1/3 -> cmdn/window1/power1 toggle
    client.setCallback([&voiceRecognition, &commands](const char * topic, uint8_t * payload, uint16_t len) {
        debug("Mqtt message received for: %s", topic);
    
        uint8_t indexPos = findPosFromEnd(topic, strlen(topic), '/');
        uint8_t index = atoi(topic+indexPos);

        uint8_t sigPos = findPosFromEnd(topic, indexPos, '/');

        char signature[MAX_SIGNATURE_LENGTH]{0};
        strncpy(signature, topic, MIN(sizeof(signature) - 1, indexPos - sigPos));

        if (train(voiceRecognition, index, signature) == 0) {
            info("Trained %s", signature);
            commands[index] = VoiceMqtt::fromPayload(payload, len);
            loadGroup(voiceRecognition, 0);

            EEPROM.put(EEPROM_CACHE, commands);
            if (!EEPROM.commit()) {
                error("Failed to save commands.");
            }
        }
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

        if (strncmp("group", (char*)buf+4, 5) == 0) {
            uint8_t group = atoi((char*)buf+9);
            loadGroup(voiceRecognition, group);
        } else if (COUNT_OF(commands) > buf[1] && commands[buf[1]].topic[0] != '\0') {
            if (!client.publish(commands[buf[1]].topic, commands[buf[1]].message)) {
                error("Failed to publish state");
            } else {
                info("Sent %s %s", commands[buf[1]].topic, commands[buf[1]].message);
            }
        }
    }

	if(millis() - lastRefreshTime >= DISPLAY_TIME) {

        if (!connectToWifi(wifi, WIFI_RETRY)) {
            error("Unable to connect to wifi");
        }
        resetWatchDog();
        if (!connectToMqtt(client, MQTT_CLIENT_NAME, CHANNEL_TRAIN)) {
            error("Failed to connect to mqtt server on %s", MQTT_SERVER_ADDRESS);
        }
        resetWatchDog();

		sendLiveData(client);
        
        info("Ping");
		lastRefreshTime = millis();
	}

    resetWatchDog();
}
