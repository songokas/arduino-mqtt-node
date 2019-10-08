#include <Arduino.h>
#include <RF24.h>
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
#include "RadioEncrypted/Entropy/EspRandomAdapter.h"

using MqttModule::MqttMessage;
using MqttModule::MessageType;
using MqttModule::SubscriberList;
using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::Entropy::EspRandomAdapter;

const uint16_t DISPLAY_TIME {10000};
const uint8_t MAX_SEND_RETRIES {3};
const uint8_t MAX_MESSAGE_QUEUE {10};
const uint8_t MAX_MESSAGE_FAILURES {60};

unsigned long lastRefreshTime {0};
unsigned long lastSentMessageTime {0};

const char * TOPIC_KEEP_ALIVE = "mesh-gateway/esp";

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, SHARED_KEY
// int main does not work
//

ESP8266WiFiMulti WiFiMulti;
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

SubscriberList subscribers;

struct MessageQueueItem
{
    uint16_t node {0};
    MqttMessage message;
    bool initialized {false};
    uint8_t failedToSend {0};
};

MessageQueueItem messageQueue[MAX_MESSAGE_QUEUE];

#include "helpers.h"

void setup() {


    Serial.begin(9600);
    ESP.wdtDisable();
    ESP.wdtEnable(10000);

    // We start by connecting to a WiFi network
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(WLAN_SSID_1, WLAN_PASSWORD_1);
    #ifdef WLAN_SSID_2
    WiFiMulti.addAP(WLAN_SSID_2, WLAN_PASSWORD_2);
    #endif

    Serial << F("Wait for WiFi... ") << endl;

    while (WiFiMulti.run() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
        ESP.wdtFeed();
    }

    Serial << F("Wifi connected") << endl;
    Serial << F("IP address: ") << WiFi.localIP() << endl;
    delay(500);

    mesh.setNodeID(0);

    radio.setPALevel(RF24_PA_LOW);

    ESP.wdtFeed();

    Serial << F("Connecting to mesh") << endl;
    if (!mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT)) {
        Serial << F("Failed to connect to mesh") << endl;
    }
    ESP.wdtFeed();

    client.setServer(MQTT_SERVER_ADDRESS, 1883);

    client.setCallback([&mesh, &subscribers, &encMesh](const char * topic, uint8_t * payload, uint16_t len) {
    
        DPRINT(F("Mqtt message received for: ")); DPRINTLN(topic);
        auto subscriber = subscribers.getSubscriber(topic);
        if (!subscriber) {
            DPRINT(F("No nodes subscribed for "));
            DPRINTLN(topic);
            return;
        }
        for (auto node: subscriber->nodes) {
            if (!(node > 0)) {
                continue;
            }
            MqttMessage message(topic);
            memcpy(message.message, payload, MIN(len, COUNT_OF(message.message)));
            if (!addToQueue(message, node)) {
                DPRINT(F("Failed to add to queue"));
            }
        }
    });
    reconnect();
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
                DPRINT(F("Subscribed for: ")); DPRINTLN(message.topic);
                DPRINT(F("Node: ")); DPRINTLN(fromNode);

            } else if (header.type == (uint8_t)MessageType::Publish) {
                // push to the server
                if (!client.publish(message.topic, message.message, true)) {
                    Serial << F("Failed to send message") << endl;
                }
                DPRINT(F("Publish: ")); DPRINTLN(message.topic);
                DPRINT(F("Message: ")); DPRINTLN(message.message);
            }

        } else {
            DPRINTLN(F("Failed to receive packets"));
        }
        ESP.wdtFeed();
    }

    if (millis() - lastSentMessageTime >= 1500) {
        uint8_t messagesSent = sendMessages();
        if (messagesSent > 0) {
            DPRINT(F("Messages sent ")); DPRINTLN(messagesSent);
        }
        lastSentMessageTime = millis();
    }

	if(millis() - lastRefreshTime >= DISPLAY_TIME) {
		lastRefreshTime += DISPLAY_TIME;

        char liveMsg[16] {0};
        sprintf(liveMsg, "%d", millis());
		if (!client.publish(TOPIC_KEEP_ALIVE, liveMsg)) {
            Serial << F("Failed to send keep alive") << endl;
		}

        Serial << (F("Ping")) << endl;
        reconnect();
	}

    ESP.wdtFeed();
}
