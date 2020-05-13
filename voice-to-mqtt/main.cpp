#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <VoiceRecognitionV3.h>

#include "CommonModule/MacroHelper.h"
#include "MqttModule/MqttConfig.h"
#include "RadioEncrypted/Helpers.h"

using RadioEncrypted::connectToWifi;
using RadioEncrypted::resetWatchDog;
using RadioEncrypted::connectToMqtt;
using RadioEncrypted::sendLiveData;

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

struct VoiceMqtt
{
    uint8_t index {0};
    const char * topic {nullptr};
    bool value {false};

    VoiceMqtt(uint8_t index, const char * topic, bool value): index(index), topic(topic), value(value) {}
};

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);
HTTPClient httpClient;
VR voiceRecognition(PIN_TX, PIN_RX);

const VoiceMqtt commands[] {
    {1, "voice1/laistymas/POWER1", true},
};

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


    if(voiceRecognition.clear() == 0) {
        info("Recognizer cleared.");
    } else {
        error("VoiceRecognitionModule not found.");
        error("Please check connection and restart Arduino.");
        ESP.deepSleep(120e6);
    }
  
    for (auto command: commands) {
        resetWatchDog();
        if(!(voiceRecognition.load(command.index) >= 0)) {
            error("failed to load record: %d", command.index);
        }
    }

}

void loop()
{
    client.loop();

    uint8_t buf[64] {0};
    uint8_t ret = voiceRecognition.recognize(buf, 50);
    if(ret > 0) {
        bool found {false};
        for (auto command: commands) {
            // buf[1] is train index
            if (command.index == buf[1]) {

                if (!client.publish(command.topic, command.value ? "1" : "0")) {
                    error("Failed to publish state");
                } else {
                    info("Sent %s %d", command.topic, command.value);
                }

                found = true;
                break;
            }
        }
        if (!found) {
            //warning("Record function undefined");
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
