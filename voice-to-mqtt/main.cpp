#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
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

const uint8_t MAX_TOPIC {40};
const uint8_t MAX_MESSAGE {20};
const char DEFAULT_MESSAGE[] PROGMEM {"toggle"};

#include "VoiceMqtt.h"

const uint16_t DISPLAY_TIME {60000};
const uint8_t WIFI_RETRY {10};
const uint8_t MQTT_RETRY {6};
const uint8_t PIN_TX {D2};
const uint8_t PIN_RX {D3};
const uint32_t BAUD_RATE_VOICE {9600};
const uint32_t BAUD_RATE {115200};
const uint16_t RESET_TIME {10000};
const uint8_t MAX_LOADED {7};
const uint8_t MAX_RECOGNIZED_BUFFER {64 + 1};
const char CHANNEL_TRAIN[] {"voice/train/#"};
const uint8_t MAX_SIGNATURE_LENGTH {10};
const uint8_t MAX_COMMANDS {16};
const uint16_t EEPROM_CACHE {4000};
const uint16_t EEPROM_CACHE_CONFIRM {EEPROM_CACHE - 5};
const uint8_t TRIGGER_COMMAND_GROUP {2};

unsigned long lastRefreshTime {0};
bool reloadRecognizer {false};

#include "helpers.h"

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);
VR voiceRecognition(PIN_TX, PIN_RX);
VoiceMqtt commands[MAX_COMMANDS] {};

void setup()
{
    Serial.begin(BAUD_RATE);
    voiceRecognition.begin(BAUD_RATE_VOICE);
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

    if (EEPROM.read(EEPROM_CACHE_CONFIRM) == 1) {
        EEPROM.get(0, commands);
    } else {
        info("Using initial commands");
        // initial predefined commands or provide custom-commands.h file
        // commands[0] = VoiceMqtt::fromFlashString(PSTR("cmnd/heating/power1"), PSTR("toggle"));
        #ifdef CUSTOM_COMMANDS
        #include "custom-commands.h"
        #endif
        EEPROM.put(0, commands);
        EEPROM.write(EEPROM_CACHE_CONFIRM, 1);
        if (!EEPROM.commit()) {
            error("Failed to save commands.");
        }
    }

    uint8_t i {0};
    for (const auto & command: commands) {
        debug("Command %d %s %s", i, command.topic, command.message);
        i++;
    }

    //expects topic -> message : voice/train/sign1/3 -> cmdn/window1/power1 toggle
    client.setCallback([&voiceRecognition, &commands](const char * topic, uint8_t * payload, uint16_t len) {
        debug("Mqtt message received for: %s", topic);
    
        uint8_t indexPos = findPosFromEnd(topic, strlen(topic), '/');
        if (indexPos == 0) {
            warning("Ignoring training. Topic %s does not have index.", topic);
            return;
        }
        uint8_t index = atoi(topic+indexPos+1);
        if (COUNT_OF(commands) <= index) {
            warning("Ignoring training. Topic %s index %d out of bounds %d.", topic, index, COUNT_OF(commands));
            return;
        }

        uint8_t sigPos = findPosFromEnd(topic, indexPos - 1, '/');

        if (sigPos == 0) {
            warning("Ignoring training. Topic %s does not have signature. Expected topic/signature/index", topic);
            return;
        }

        char signature[MAX_SIGNATURE_LENGTH]{0};
        strncpy(signature, topic+sigPos+1, MIN(sizeof(signature) - 1, indexPos - sigPos - 1));
        if (!(strlen(signature) > 0)){
            debug("Ignoring training. Sinature is empty", signature);
            return;
        }

        if (train(voiceRecognition, index, signature) >= 0) {
    
            commands[index] = VoiceMqtt::fromPayload(payload, len);

            info("Trained %d %s %s %s", index, signature, commands[index].topic, commands[index].message);

            EEPROM.put(sizeof(commands[index]) * index, commands[index]);
            if (!EEPROM.commit()) {
                error("Failed to save commands.");
            }
        }
    });

    if (!loadGroup(voiceRecognition, TRIGGER_COMMAND_GROUP)) {
        error("VoiceRecognitionModule unable to load group: %d", TRIGGER_COMMAND_GROUP);
        reloadRecognizer = true;
    }
}

void loop()
{
    client.loop();

    uint8_t buf[MAX_RECOGNIZED_BUFFER] {0};
    uint8_t ret = voiceRecognition.recognize(buf, 50);

    if(ret > 0 && ret != 255) {
        // signatures with group1 will trigger will load 7-13 records
        if (strncmp("group", (char*)buf+4, 5) == 0) {
            uint8_t group = atoi((char*)buf+9);
            if (!loadGroup(voiceRecognition, group)) {
                error("VoiceRecognitionModule unable to load group: %d", group);
                reloadRecognizer = true;
            }
        } else {
            if (COUNT_OF(commands) > buf[1] && commands[buf[1]].topic[0] != '\0') {
                if (!client.publish(commands[buf[1]].topic, commands[buf[1]].message)) {
                    error("Failed to publish state");
                } else {
                    info("Sent %s %s", commands[buf[1]].topic, commands[buf[1]].message);
                }
            } else {
                warning("Unknown voice index %d", buf[1]);
            }
            // back to the initial trigger commands
            if (!loadGroup(voiceRecognition, TRIGGER_COMMAND_GROUP)) {
                error("VoiceRecognitionModule unable to load group: %d", TRIGGER_COMMAND_GROUP);
                reloadRecognizer = true;
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

        if (reloadRecognizer && loadGroup(voiceRecognition, TRIGGER_COMMAND_GROUP)) {
            reloadRecognizer = false;
        }

        info("Ping");
		lastRefreshTime = millis();
	}

    resetWatchDog();
}
