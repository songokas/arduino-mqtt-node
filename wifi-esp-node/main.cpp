#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Streaming.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// satisfy arduino-builder
#include "ArduinoBuilderMqttModule.h"
#include "ArduinoBuilderMessageHandlers.h"
#include "ArduinoBuilderValueProviders.h"
// end
//
#include "CommonModule/MacroHelper.h"
#include "MqttModule/SubscriberList.h"
#include "MqttModule/MqttMessage.h"
#include "MqttModule/PinCollection.h"
#include "MqttModule/MessageHandlers/SubscribePubSubHandler.h"
#include "MqttModule/MessageHandlers/PinStateHandler.h"
#include "MqttModule/MessageHandlers/PinStateJsonHandler.h"
#include "MqttModule/ValueProviders/AnalogProvider.h"
#include "MqttModule/ValueProviders/DigitalProvider.h"
#include "MqttModule/ValueProviders/ValueProviderFactory.h"
#include "MqttModule/ValueProviders/DallasTemperatureProvider.h"

using MqttModule::SubscriberList;
using MqttModule::Subscriber;
using MqttModule::Pin;
using MqttModule::PinCollection;
using MqttModule::MqttMessage;
using MqttModule::MessageHandlers::IMessageHandler;
using MqttModule::MessageHandlers::PinStateHandler;
using MqttModule::MessageHandlers::PinStateJsonHandler;
using MqttModule::MessageHandlers::SubscribePubSubHandler;
using MqttModule::ValueProviders::IValueProvider;
using MqttModule::ValueProviders::ValueProviderFactory;
using MqttModule::ValueProviders::DigitalProvider;
using MqttModule::ValueProviders::AnalogProvider;
using MqttModule::ValueProviders::DallasTemperatureProvider;

const uint16_t DISPLAY_TIME {60000};
const uint8_t TEMPERATURE_PIN = 2;

unsigned long lastRefreshTime {0};

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, SLEEP_FOR
// int main does not work

ESP8266WiFiMulti WiFiMulti;
WiFiClient net;
PubSubClient client(net);

IMessageHandler * handlers1[] {nullptr, nullptr};
IMessageHandler * handlers2[] {nullptr, nullptr};
uint16_t nodes1[] {0, 0};
uint16_t nodes2[] {0, 0};
Subscriber subs[] { {handlers1, COUNT_OF(handlers1), nodes1, COUNT_OF(nodes1)}, {handlers2, COUNT_OF(handlers2), nodes2, COUNT_OF(nodes2)}};

SubscriberList subscribers (subs, COUNT_OF(subs));

Pin pins[] {{TEMPERATURE_PIN, "temperature"}};
PinCollection pinCollection(pins, COUNT_OF(pins));

OneWire oneWire(TEMPERATURE_PIN); 
DallasTemperature sensor(&oneWire);

DallasTemperatureProvider temperatureProvider(sensor);
AnalogProvider analogProvider;
DigitalProvider digitalProvider;

IValueProvider * providers[] {&temperatureProvider, &analogProvider, &digitalProvider};
ValueProviderFactory valueProviderFactory(providers, COUNT_OF(providers));

PinStateHandler handler(pinCollection, valueProviderFactory);
SubscribePubSubHandler subscribeHandler(client, subscribers, handler);
PinStateJsonHandler jsonHandler(pinCollection, valueProviderFactory);

#include "helpers.h"

void setup() {
    Serial.begin(9600);
    // ESP.wdtDisable();
    // ESP.wdtEnable(10000);

    // We start by connecting to a WiFi network
    WiFi.mode(WIFI_STA);
    WiFiMulti.addAP(WLAN_SSID_1, WLAN_PASSWORD_1);
    #ifdef WLAN_SSID_2
    WiFiMulti.addAP(WLAN_SSID_2, WLAN_PASSWORD_2);
    #endif

    Serial << F("Wait for WiFi... ") << endl;

    uint16_t wifiAttempts = 0;
    while (WiFiMulti.run() != WL_CONNECTED) {
        wifiAttempts++;
        Serial.print(".");
        delay(500 * wifiAttempts);
        ESP.wdtFeed();
    }

    Serial << F("Wifi connected") << endl;
    Serial << F("IP address: ") << WiFi.localIP() << endl;
    delay(500);

    ESP.wdtFeed();

    Serial << F("Wait for mqtt server on ") << MQTT_SERVER_ADDRESS << endl;

    uint16_t mqttAttempts = 0;
    client.setServer(MQTT_SERVER_ADDRESS, 1883);
    while (!client.connect(NODE_NAME)) {
        mqttAttempts++;
        Serial.print(".");
        delay(500 * mqttAttempts);
        ESP.wdtFeed();
    }

    Serial << F("connected.") << endl;

#ifndef SLEEP_FOR
    subscribeToChannels(client, subscribers, subscribeHandler, jsonHandler);

    client.setCallback([&subscribers](const char * topic, uint8_t * payload, uint16_t len) {
        
        Serial << F("Mqtt message received for: ") << topic << endl;
        MqttMessage message(topic);
        memcpy(message.message, payload, MIN(len, COUNT_OF(message.message)));
        if (!(subscribers.call(message) > 0)) {
            Serial << "Not handled: " << message.topic << endl; 
        }
    });
#endif

}

void loop()
{
#ifdef SLEEP_FOR
    for (auto & pin: pins) {
        sendStateData(client, valueProviderFactory, pin);
    }
    client.loop();
    Serial << F("Sleeping: ") << SLEEP_FOR << endl;
    ESP.deepSleep(SLEEP_FOR);
#else
    client.loop();

    for (auto & pin: pins) {
        if (millis() - pin.lastRead > pin.readInterval) {
            //Serial << F("Pin: ") << pin.id << F(" ") << pin.type << endl;
            pin.lastRead = millis();
            sendStateData(client, valueProviderFactory, pin);
            ESP.wdtFeed();
        } 
    }

	if(millis() - lastRefreshTime >= DISPLAY_TIME) {
		lastRefreshTime += DISPLAY_TIME;

		sendLiveData(client);

        Serial << (F("Ping")) << endl;
        
        if (!client.connected()) {
            if (client.connect(NODE_NAME)) {
                Serial << F("Mqtt reconnected") << endl;
                subscribeToChannels(client, subscribers, subscribeHandler, jsonHandler);
            } else {
                Serial << F("Mqtt failed to reconnect") << endl; 
            }
        }
        
	}
    ESP.wdtFeed();
#endif
}
