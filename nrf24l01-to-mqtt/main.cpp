#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <RF24.h>
#include <RF24Network.h>
#include <SPI.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <SPI.h>
#include <PubSubClient.h>

// satisfy arduino-builder
#include "ArduinoBuilderRadioEncrypted.h"
//#include "ArduinoBuilderMessageHandlers.h"
#include "ArduinoBuilderMqttModule.h"
#include "ArduinoBuilderEntropy.h"
// end
//
#include "CommonModule/MacroHelper.h"
#include "MqttModule/MqttMessage.h"

#define ENCRYPTION_MAX_USER_DATA_LENGTH MQTT_MAX_LEN_TOPIC + MQTT_MAX_LEN_MESSAGE

#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedNetwork.h"
#include "RadioEncrypted/Entropy/AnalogSignalEntropy.h"
#include "RadioEncrypted/Helpers.h"

using MqttModule::MqttMessage;
using MqttModule::MessageQueueItem;
using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedNetwork;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::connectToNetwork;
using RadioEncrypted::connectToMqtt;
using RadioEncrypted::sendLiveData;
using RadioEncrypted::connectToWifi;
using RadioEncrypted::resetWatchDog;

#include "helpers.h"

#ifndef USE_ENTROPY_PIN
#define USE_ENTROPY_PIN 3
#endif
#ifndef USE_BAUD_RATE
#define USE_BAUD_RATE 9600
#endif
#ifndef WATCHDOG_RESET_TIME
#define WATCHDOG_RESET_TIME 8000
#endif

const uint16_t BAUD_RATE {USE_BAUD_RATE};
const uint16_t WATCHDOG_RESET {WATCHDOG_RESET_TIME};
const char * WLAN_SSID_1 {WIFI_SSID_1};
const char * WLAN_PASSWORD_1 {WIFI_PASSWORD_1};
const char * MQTT_SERVER {MQTT_SERVER_ADDRESS};
const char * NODE_NAME {MQTT_CLIENT_NAME}; // mqtt client name
const uint16_t NODE_ID {NRF_NODE_ID}; // nrf24 network id
const char SHARED_KEY[ENCRYPTION_KEY_LENGTH + 1] {ENCRYPTION_KEY}; // nrf24 network encryption key 16 chars
const uint8_t ENTROPY_PIN {USE_ENTROPY_PIN}; // pin used for analog entropy retrieval
const uint8_t WIFI_RETRY = 6;
const uint8_t MAX_QUEUE_FOR_FAILURES = 60;

bool connectedToNrfNetwork {false};
unsigned long monitorTime {0};
unsigned long lastSentMessageTime {0};

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);

RF24 radio(D4, D8);
RF24Network network(radio);

Acorn128 cipher;
AnalogSignalEntropy entropy(ENTROPY_PIN, NODE_ID);
Encryption encryption (cipher, SHARED_KEY, entropy);
EncryptedNetwork encNetwork(NODE_ID, network, encryption);
MessageQueueItem messageQueue[MAX_QUEUE_FOR_FAILURES];

bool connectCallback()
{
    // if we cant connect to wifi no reason to continue
    if (!connectToWifi(wifi, WIFI_RETRY)) {
        error("Unable to connect to wifi");
        return false;
    }
    resetWatchDog();
    delay(500);

    if ((!connectedToNrfNetwork || radio.failureDetected) && !connectToNetwork(network, radio, NODE_ID, RADIO_CHANNEL)) {
        error("Unable to connect to nrf24 network");
        return false;
    } else {
        connectedToNrfNetwork = true;
    }

    resetWatchDog();
    delay(500);

    if (!connectToMqtt(client, NODE_NAME, CHANNEL_MQTT_TO_NRF_NETWORK)) {
        error("Failed to connect to mqtt server on %s", MQTT_SERVER);
        return false;
    }
    resetWatchDog();
    return true;
}


void setup()
{
    Serial.begin(BAUD_RATE);
    ESP.wdtDisable();
    ESP.wdtEnable(WATCHDOG_RESET_TIME);

    WiFi.mode(WIFI_STA);
    wifi.addAP(WLAN_SSID_1, WLAN_PASSWORD_1);
    #ifdef WLAN_SSID_2
    wifi.addAP(WLAN_SSID_2, WLAN_PASSWORD_2);
    #endif

    client.setServer(MQTT_SERVER, 1883);

    #ifdef MQTT_TO_NRF_NETWORK
        // topic received 
        // nrfNetwork/{nodeId}/{topic} message
        // will be forwarded to nodeId as {topic} message
        // e.g. nrfNetwork/132/heating/nodes/bedroom 1 => heating/nodes/bedroom 1
        // e.g. nrfNetwork/431/sensor1 on => sensor1 on
        client.setCallback([&mesh, &subscribers, &encMesh, &messageQueue](const char * topic, uint8_t * payload, uint16_t len) {
            MqttMessage message(topic + findNextPos(topic, strlen(topic), '/', 2));
            memcpy(message.message, payload, MIN(len, COUNT_OF(message.message)));
            if (!encNetwork.send(&message, sizeof(message), 0, node)) {
                error("Failed to send to node %d", node);
                if (!addToQueue(messageQueue, COUNT_OF(messageQueue), message, node)) {
                    error("Failed to add to queue");
                }
            } else {
                debug("Mqtt message received for: %s", topic);
            }

        });
    #endif
    connectCallback();
}

void loop()
{
    network.update();
    client.loop();

    if (encNetwork.isAvailable()) {
        if (!forwardToMqtt(encNetwork, client)) {
            error("Failed to forward message");
        }
    }
    if (millis() - monitorTime < 60000UL) {
        if (connectCallback()) {
            sendLiveData(client);
        }
        monitorTime = millis();
    }
    if (millis() - lastSentMessageTime >= 1500) {
        uint8_t messagesSent = sendMessages(encNetwork, messageQueue, COUNT_OF(messageQueue));
        if (messagesSent > 0) {
            info("Messages sent %d", messagesSent);
        }
        lastSentMessageTime = millis();
    }
    delay(5);
    resetWatchDog();
}
