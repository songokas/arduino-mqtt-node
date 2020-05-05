#include <Arduino.h>
#include <avr/wdt.h>
#include <RF24.h>
#include <RF24Network.h>
#include <RF24Mesh.h>
#include <Crypto.h>
#include <CryptoLW.h>
#include <Acorn128.h>
#include <Entropy.h>
#include <Streaming.h>
// make .mk compiler happy
#include <Wire.h>
#include <AuthenticatedCipher.h>
#include <Cipher.h>
#include <SPI.h>
//#include <OneWire.h>
//
#include <MemoryFree.h>
#include <ArduinoJson.h>

#include "CommonModule/MacroHelper.h"
#include "RadioEncrypted/RadioEncryptedConfig.h"
#include "RadioEncrypted/Helpers.h"
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedNetwork.h"
#include "RadioEncrypted/Entropy/AvrEntropyAdapter.h"

#include "MqttModule/MeshMqttClient.h"
#include "MqttModule/SubscriberList.h"
#include "MqttModule/MqttMessage.h"
#include "MqttModule/PinCollection.h"
#include "MqttModule/MessageHandlers/SubscribeHandler.h"
#include "MqttModule/MessageHandlers/PinStateJsonHandler.h"
#include "MqttModule/MessageHandlers/PinStateHandler.h"
#include "MqttModule/ValueProviders/AnalogProvider.h"
#include "MqttModule/ValueProviders/DigitalProvider.h"
#include "MqttModule/ValueProviders/ValueProviderFactory.h"

using RadioEncrypted::Encryption;
using RadioEncrypted::EncryptedNetwork;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AvrEntropyAdapter;

using MqttModule::MeshMqttClient;
using MqttModule::SubscriberList;
using MqttModule::StaticSubscriberList;
using MqttModule::Pin;
using MqttModule::MqttMessage;
using MqttModule::PinCollection;
using MqttModule::StaticPinCollection;
using MqttModule::MessageHandlers::SubscribeHandler;
using MqttModule::MessageHandlers::PinStateJsonHandler;
using MqttModule::MessageHandlers::PinStateHandler;
using MqttModule::ValueProviders::IValueProvider;
using MqttModule::ValueProviders::ValueProviderFactory;
using MqttModule::ValueProviders::DigitalProvider;
using MqttModule::ValueProviders::AnalogProvider;
using RadioEncrypted::connectToNetwork;
using RadioEncrypted::resetWatchDog;

#ifdef CUSTOM_PROVIDERS
#include "CustomValueProviders/ValueProvidersInclude.h"
#endif

#include "helpers.h"

const uint16_t DISPLAY_TIME {30000};
const uint16_t BAUD_RATE {9600};
const uint8_t CE_PIN {7};
const uint8_t CN_PIN {8};
const char * SHARED_KEY {ENCRYPTION_KEY};
const uint16_t NODE_ID {NRF_NODE_ID};
const uint8_t MAX_SUBSCRIBERS {2};
const uint8_t MAX_NODES_PER_SUBSCRIBER {2};
const uint8_t MAX_HANDLERS_PER_SUBSCRIBER {2};

int main()
{
  init();
  Serial.begin(BAUD_RATE);

  info("freeMemory %d", freeMemory());

  wdt_enable(WDTO_8S);

  RF24 radio(CE_PIN, CN_PIN);
  RF24Network network(radio);
  RF24Mesh mesh(radio, network);

  Acorn128 cipher;
  EntropyClass entropy;
  entropy.initialize();
  AvrEntropyAdapter entropyAdapter(entropy);
  Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
  EncryptedNetwork encMesh (NODE_ID, network, encryption);

  StaticSubscriberList<MAX_SUBSCRIBERS, MAX_NODES_PER_SUBSCRIBER, MAX_HANDLERS_PER_SUBSCRIBER> subscribers;
  MeshMqttClient client(encMesh, subscribers);

  Pin pins [] {AVAILABLE_PINS};
  StaticPinCollection<COUNT_OF(pins)> pinCollection(pins);

  AnalogProvider analogProvider;
  DigitalProvider digitalProvider;

  IValueProvider * providers[] {&analogProvider, &digitalProvider};
  ValueProviderFactory valueProviderFactory(providers, COUNT_OF(providers));

  PinStateHandler handler(pinCollection, valueProviderFactory);
  SubscribeHandler subscribeHandler(client, handler);
  PinStateJsonHandler jsonHandler(pinCollection, valueProviderFactory);

#ifdef CUSTOM_PROVIDERS
#include "CustomValueProviders/ValueProvidersFactory.h"
#endif

  bool connectedToNrfNetwork = false;
  unsigned long lastRefreshTime = 0;
  unsigned long lastSubscribeTime = 0;

  if (!connectToNetwork(network, radio, NODE_ID, RADIO_CHANNEL)) {
      error("Unable to connect to nrf24 network");
  } else {
    connectedToNrfNetwork = true;
  }
  resetWatchDog();

  while (true) {

    mesh.update();
    client.loop();

    for (auto & pin: pins) {
        if (!(pin.id > 0)) {
            continue;
        }
        if (pin.changed) {
            pin.changed = false;
            sendStateData(client, valueProviderFactory, pin);
            resetWatchDog();
        } 
    }

	if (millis() - lastRefreshTime >= DISPLAY_TIME) {

        info("freeMemory %d", freeMemory());

        if ((!connectedToNrfNetwork || radio.failureDetected) && !connectToNetwork(network, radio, NODE_ID, RADIO_CHANNEL)) {
            error("Unable to connect to nrf24 network");
        } else {
            connectedToNrfNetwork = true;
        }

        sendLiveData(client);

        info("Ping");
		lastRefreshTime = millis();
	}

    if (millis() > lastSubscribeTime)  {
        lastSubscribeTime = millis() + subscribeToChannels(client, subscribeHandler, jsonHandler);
    }

    resetWatchDog();

  }
}
