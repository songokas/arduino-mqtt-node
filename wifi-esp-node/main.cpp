#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Streaming.h>
#include <SPI.h>
#include <PubSubClient.h>

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

using MqttModule::SubscriberList;
using MqttModule::Pin;
using MqttModule::PinType;
using MqttModule::PinCollection;
using MqttModule::MqttMessage;
using MqttModule::MessageHandlers::PinStateHandler;
using MqttModule::MessageHandlers::PinStateJsonHandler;
using MqttModule::MessageHandlers::SubscribePubSubHandler;
using MqttModule::ValueProviders::ValueProviderFactory;
using MqttModule::ValueProviders::DigitalProvider;
using MqttModule::ValueProviders::AnalogProvider;

#ifdef CUSTOM_PROVIDERS
#include "CustomValueProviders/ValueProvidersInclude.h"
#endif

const uint16_t DISPLAY_TIME {10000};
unsigned long lastRefreshTime {0};

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, SHARED_KEY
// int main does not work
//

ESP8266WiFiMulti WiFiMulti;
WiFiClient net;
PubSubClient client(net);

SubscriberList subscribers;

Pin pins[] {AVAILABLE_PINS};
PinCollection pinCollection(pins, COUNT_OF(pins));

ValueProviderFactory valueProviderFactory;

AnalogProvider analogProvider;
DigitalProvider digitalProvider;


PinStateHandler handler(pinCollection, valueProviderFactory);
SubscribePubSubHandler subscribeHandler(client, subscribers, handler);
PinStateJsonHandler jsonHandler(pinCollection, valueProviderFactory);

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

    ESP.wdtFeed();

    valueProviderFactory.addByType((uint16_t)PinType::Analog, &analogProvider);
    valueProviderFactory.addByType((uint16_t)PinType::Digital, &digitalProvider);

#ifdef CUSTOM_PROVIDER
#include "CustomValueProviders/ValueProvidersFactory.h"
#endif

    subscribeToChannels(client, subscribers, subscribeHandler, jsonHandler);

    client.setServer(MQTT_SERVER_ADDRESS, 1883);

    client.setCallback([&subscribers](const char * topic, uint8_t * payload, uint16_t len) {
        
        DPRINT(F("Mqtt message received for: ")); DPRINTLN(topic);
        MqttMessage message(topic);
        memcpy(message.message, payload, MIN(len, COUNT_OF(message.message)));
        if (!(subscribers.call(message) > 0)) {
            Serial << "Not handled: " << message.topic << endl; 
        }
    });
}

void loop()
{
    client.loop();

    for (auto & pin: pins) {
        if (!(pin.id > 0)) {
            continue;
        }
        if (millis() - pin.lastRead > pin.readInterval) {
            pin.lastRead = millis();
            sendStateData(client, valueProviderFactory, pin);
            ESP.wdtFeed();
        } 
    }

	if(millis() - lastRefreshTime >= DISPLAY_TIME) {
		lastRefreshTime += DISPLAY_TIME;

		sendLiveData(client);

        Serial << (F("Ping")) << endl;
        reconnect();
	}

    ESP.wdtFeed();
}
