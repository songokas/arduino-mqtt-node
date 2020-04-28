#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include <SPI.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Streaming.h>
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
#include "MqttModule/SubscriberList.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedMesh.h"
#include "RadioEncrypted/Helpers.h"
#include "RadioEncrypted/Entropy/EspRandomAdapter.h"

using MqttModule::MqttMessage;
using MqttModule::MessageType;
using MqttModule::SubscriberList;
using MqttModule::StaticSubscriberList;
using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::Entropy::EspRandomAdapter;
using RadioEncrypted::connectToMesh;
using RadioEncrypted::connectToMqtt;
using RadioEncrypted::sendLiveData;
using RadioEncrypted::connectToWifi;

struct MessageQueueItem
{
    uint16_t node {0};
    MqttMessage message;
    bool initialized {false};
    uint8_t failedToSend {0};
};

const uint8_t MAX_SEND_RETRIES {3};
const uint8_t MAX_MESSAGE_QUEUE {10};
const uint8_t MAX_MESSAGE_FAILURES {60};
const uint8_t WIFI_RETRY = 10;
const uint8_t MQTT_RETRY = 6;

unsigned long lastRefreshTime {0};
unsigned long lastSentMessageTime {0};

uint8_t publishFailed {0};
uint8_t reconnectMqttFailed {0};

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, SHARED_KEY, MAX_SUBSCRIBERS, MAX_NODES_PER_TOPIC
// int main does not work

ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);

RF24 radio(D4, D8);
RF24Network network(radio);
RF24Mesh mesh(radio,network);

Acorn128 cipher;
ESP8266TrueRandomClass entropy;
EspRandomAdapter entropyAdapter(entropy);
Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
EncryptedMesh encMesh (mesh, network, encryption);

StaticSubscriberList<MAX_SUBSCRIBERS, 2, MAX_NODES_PER_TOPIC> subscribers;

MessageQueueItem messageQueue[MAX_MESSAGE_QUEUE];

#include "helpers.h"

void setup()
{

    Serial.begin(9600);
    ESP.wdtDisable();
    ESP.wdtEnable(10000);

    // We start by connecting to a WiFi network
    WiFi.mode(WIFI_STA);
    wifi.addAP(WLAN_SSID_1, WLAN_PASSWORD_1);
    #ifdef WLAN_SSID_2
    wifi.addAP(WLAN_SSID_2, WLAN_PASSWORD_2);
    #endif

    if (!connectToWifi(wifi, WIFI_RETRY)) {
        error("Unable to connect to wifi. Sleeping..");
        ESP.deepSleep(120e6);
    }
    delay(500);

    mesh.setNodeID(0);

    RESET_WATCHDOG();

    if (!connectToMesh(mesh)) {
        error("Unable to connect to mesh. Sleeping..");
        ESP.deepSleep(120e6);
    }

    radio.setPALevel(RF24_PA_HIGH);

    RESET_WATCHDOG();

    client.setServer(MQTT_SERVER_ADDRESS, 1883);

    reconnectMqttFailed = connectToMqtt(client, NODE_NAME, nullptr) ? 0 : 1;

    client.setCallback([&mesh, &subscribers, &encMesh, &messageQueue](const char * topic, uint8_t * payload, uint16_t len) {
    
        debug("Mqtt message received for: %s", topic);
        auto subscriber = subscribers.getSubscribed(topic);
        if (!subscriber) {
            warning("No nodes subscribed for %s", topic);
            return;
        }
        for (uint8_t i = 0; i < subscriber->getNodeArrLength(); i++) {
            auto node = subscriber->getNodeByIndex(i);
            if (!(node > 0)) {
                continue;
            }
            MqttMessage message(topic);
            memcpy(message.message, payload, MIN(len, COUNT_OF(message.message)));
            if (!addToQueue(messageQueue, COUNT_OF(messageQueue), message, node)) {
                error("Failed to add to queue");
            }
        }
    });
}

void loop()
{
    mesh.update();
    mesh.DHCP();
    client.loop();

    while (encMesh.isAvailable()) {

        MqttMessage message;
        RF24NetworkHeader header;

        if (encMesh.receive(&message, sizeof(message), (uint8_t)MessageType::All, header)) {

            if (header.type == (uint8_t)MessageType::Subscribe && !subscribers.hasSubscribed(message.topic)) {
                uint16_t fromNode = mesh.getNodeID(header.from_node);
                subscribers.add(message.topic, nullptr, fromNode);
                //subscribe locally
                client.subscribe(message.topic);
                info("Subscribed for: %s nodeI: %d", message.topic, fromNode);

            } else if (header.type == (uint8_t)MessageType::Publish) {
                // push to the server
                if (!client.publish(message.topic, message.message, true)) {
                    error("Failed to send message");
                }
                debug("Publish topic: %s Message: %s", message.topic, message.message);
            }

        } else {
            error("Failed to receive packets");
        }
        RESET_WATCHDOG();
    }

    if (millis() - lastSentMessageTime >= 1500) {
        uint8_t messagesSent = sendMessages(messageQueue, COUNT_OF(messageQueue));
        if (messagesSent > 0) {
            info("Messages sent %d", messagesSent);
        }
        lastSentMessageTime = millis();
    }

	if (millis() - lastRefreshTime >= 30000) {

        publishFailed += sendLiveData(client) ? 0 : 1;

        debug("Ping");

        reconnectMqttFailed = connectToMqtt(client, NODE_NAME, nullptr);

        lastRefreshTime = millis();

        if (reconnectMqttFailed > 10 || publishFailed > 10 || radio.failureDetected) {
            ESP.restart();
        }
	}
    RESET_WATCHDOG();
}
