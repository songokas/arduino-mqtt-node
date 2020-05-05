#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <Streaming.h>
#include <SPI.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>

// satisfy arduino-builder
#include "ArduinoBuilderMqttModule.h"
#include "ArduinoBuilderMessageHandlers.h"
#include "ArduinoBuilderValueProviders.h"
// end
//
#include "CommonModule/MacroHelper.h"
#include "MqttModule/MqttConfig.h"
#include "RadioEncrypted/Helpers.h"
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
using MqttModule::StaticSubscriberList;
using MqttModule::Subscriber;
using MqttModule::Pin;
using MqttModule::PinCollection;
using MqttModule::StaticPinCollection;
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
using MqttModule::ValueProviders::DallasSensor;
using RadioEncrypted::connectToWifi;
using RadioEncrypted::resetWatchDog;
using RadioEncrypted::connectToMqtt;

// requires WLAN_SSID_1, WLAN_PASSWORD_1, MQTT_SERVER_ADDRESS, SLEEP_FOR, SERVER_URL
// int main does not work

const uint16_t DISPLAY_TIME {60000};
const uint8_t TEMPERATURE_PIN = 2;
const char CHANNEL_SERVER[] PROGMEM {HTTP_SERVER_URL MQTT_CLIENT_NAME "/%s/%d"};
const uint8_t WIFI_RETRY = 10;
const uint8_t MQTT_RETRY = 6;
unsigned long lastRefreshTime {0};


ESP8266WiFiMulti wifi;
WiFiClient net;
PubSubClient client(net);
HTTPClient httpClient;

StaticSubscriberList<2, 2, 2> subscribers;

Pin pins[] {{TEMPERATURE_PIN, "temperature"}};
StaticPinCollection<COUNT_OF(pins)> pinCollection(pins);

OneWire oneWire(TEMPERATURE_PIN); 
DallasSensor sensors[] {{&oneWire, TEMPERATURE_PIN}};

DallasTemperatureProvider temperatureProvider(sensors, COUNT_OF(sensors));
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
    ESP.wdtDisable();
    ESP.wdtEnable(10000);

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

    delay(500);

    ESP.wdtFeed();

#ifdef MQTT_SERVER_ADDRESS
    client.setServer(MQTT_SERVER_ADDRESS, 1883);
    if (!connectToMqtt(client, MQTT_CLIENT_NAME, nullptr)) {
        error("failed to connect to mqtt server on %s", MQTT_SERVER_ADDRESS);
        ESP.deepSleep(120e6);
    }

    #ifndef SLEEP_FOR
        subscribeToChannels(client, subscribers, subscribeHandler, jsonHandler);

        client.setCallback([&subscribers](const char * topic, uint8_t * payload, uint16_t len) {
            debug("Mqtt message received for: %s", topic);
            MqttMessage message(topic);
            memcpy(message.message, payload, MIN(len, COUNT_OF(message.message)));
            if (!(subscribers.call(message) > 0)) {
                warning("Not handled: %s", message.topic);
            }
        });
    #endif
#endif
}

void loop()
{
#ifdef SLEEP_FOR
    for (auto & pin: pins) {
        #ifdef MQTT_SERVER_ADDRESS
        sendMqttRequest(client, valueProviderFactory, pin);
        #endif
        #ifdef HTTP_SERVER_URL
        StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;
        sendPostRequest(httpClient, valueProviderFactory, pin, doc);
        #endif
        #if !defined(MQTT_SERVER_ADDRESS) && !defined(HTTP_SERVER_URL)
            error("No handler defined for sending data pin: %d", pin.id);
        #endif
    }
    client.loop();
    info("Sleeping: %d", SLEEP_FOR);
    ESP.deepSleep(SLEEP_FOR);
#else
    client.loop();

    for (auto & pin: pins) {
        if (millis() - pin.lastRead > pin.readInterval) {
            pin.lastRead = millis();

            #ifdef MQTT_SERVER_ADDRESS
            sendMqttRequest(client, valueProviderFactory, pin);
            #endif
            #ifdef HTTP_SERVER_URL
            StaticJsonDocument<MAX_LEN_JSON_MESSAGE> doc;
            sendPostRequest(httpClient, valueProviderFactory, pin, doc);
            #endif
            #if !defined(MQTT_SERVER_ADDRESS) && !defined(HTTP_SERVER_URL)
                error("No handler defined for sending data pin: ") << endl;
            #endif

            ESP.wdtFeed();
        } 
    }

	if(millis() - lastRefreshTime >= DISPLAY_TIME) {
		lastRefreshTime += DISPLAY_TIME;

        #ifdef MQTT_SERVER_ADDRESS
		sendLiveData(client);
        
        if (!client.connected()) {
            if (client.connect(MQTT_CLIENT_NAME)) {
                info("Mqtt reconnected");
                subscribeToChannels(client, subscribers, subscribeHandler, jsonHandler);
            } else {
                info("Mqtt failed to reconnect"); 
            }
        }
        #endif
        info("Ping");
        
	}
    ESP.wdtFeed();
#endif
}
