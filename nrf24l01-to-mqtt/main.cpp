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
#include <ESP8266TrueRandom.h>

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
using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedNetwork;
using RadioEncrypted::Entropy::AnalogSignalEntropy;
using RadioEncrypted::connectToNetwork;
using RadioEncrypted::connectToMqtt;
using RadioEncrypted::sendLiveData;
using RadioEncrypted::connectToWifi;
using RadioEncrypted::resetWatchDog;


bool sendMqttMessage(PubSubClient & client, const MqttMessage & data)
{
    if (client.publish(data.topic, data.message)) {
        error("Failed to publish message. Topic: %s Message: %s", data.topic, data.message);
        return false;
    } 
    debug("Sent %s %s", data.topic, data.message);
    return true;
}

bool forwardToMqtt(EncryptedNetwork & network, PubSubClient & mqttClient)
{
    MqttMessage message;
    RF24NetworkHeader header;
    if (!network.receive(&message, sizeof(message), 0, header)) {
        error("Failed to read message");
        return false;
    }
    return sendMqttMessage(mqttClient, message);
}

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

bool connectedToNrfNetwork {false};
unsigned long monitorTime {0};

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);

RF24 radio(D4, D8);
RF24Network network(radio);

Acorn128 cipher;
AnalogSignalEntropy entropy(ENTROPY_PIN, NODE_ID);
Encryption encryption (cipher, SHARED_KEY, entropy);
EncryptedNetwork encNetwork(NODE_ID, network, encryption);

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

    if (!connectToMqtt(client, NODE_NAME, nullptr)) {
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
    delay(5);
    resetWatchDog();
}