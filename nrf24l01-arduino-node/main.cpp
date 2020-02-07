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
#include "RadioEncrypted/Encryption.h"
#include "RadioEncrypted/EncryptedMesh.h"
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
using RadioEncrypted::EncryptedMesh;
using RadioEncrypted::IEncryptedMesh;
using RadioEncrypted::Entropy::AvrEntropyAdapter;

using MqttModule::MeshMqttClient;
using MqttModule::SubscriberList;
using MqttModule::Pin;
using MqttModule::PinType;
using MqttModule::MqttMessage;
using MqttModule::PinCollection;
using MqttModule::MessageHandlers::SubscribeHandler;
using MqttModule::MessageHandlers::PinStateJsonHandler;
using MqttModule::MessageHandlers::PinStateHandler;
using MqttModule::ValueProviders::ValueProviderFactory;
using MqttModule::ValueProviders::DigitalProvider;
using MqttModule::ValueProviders::AnalogProvider;

#ifdef CUSTOM_PROVIDERS
#include "CustomValueProviders/ValueProvidersInclude.h"
#endif

#include "helpers.h"

const uint16_t DISPLAY_TIME { 30000 };
const uint16_t BAUD_RATE {9600};
const uint8_t CE_PIN = 7;
const uint8_t CN_PIN = 8;

int main()
{
  init();
  Serial.begin(BAUD_RATE);

  Serial << F("freeMemory ") << freeMemory() << endl;

  wdt_enable(WDTO_8S);

  RF24 radio(CE_PIN, CN_PIN);
  RF24Network network(radio);
  RF24Mesh mesh(radio, network);

  Acorn128 cipher;
  EntropyClass entropy;
  entropy.initialize();
  AvrEntropyAdapter entropyAdapter(entropy);
  Encryption encryption (cipher, SHARED_KEY, entropyAdapter);
  EncryptedMesh encMesh (mesh, network, encryption);
  mesh.setNodeID(NODE_ID);
  // Connect to the mesh
  Serial << F("Connecting to the mesh...") << endl;
  if (!mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT)) {
    Serial << F("Failed to connect to mesh") << endl;
  } else {
    Serial << F("Connected.") << endl;
  }
  radio.setPALevel(RF24_PA_HIGH);

  wdt_reset();

  SubscriberList subscribers;
  MeshMqttClient client(encMesh, subscribers);

  // AVAILABLE_PINS
  Pin pins [] {AVAILABLE_PINS};
  PinCollection pinCollection(pins, COUNT_OF(pins));

  ValueProviderFactory valueProviderFactory;

  AnalogProvider analogProvider;
  DigitalProvider digitalProvider;

  valueProviderFactory.addByType((uint16_t)PinType::Analog, &analogProvider);
  valueProviderFactory.addByType((uint16_t)PinType::Digital, &digitalProvider);

  PinStateHandler handler(pinCollection, valueProviderFactory);
  SubscribeHandler subscribeHandler(client, handler);
  PinStateJsonHandler jsonHandler(pinCollection, valueProviderFactory);

#ifdef CUSTOM_PROVIDERS
#include "CustomValueProviders/ValueProvidersFactory.h"
#endif

  unsigned long lastRefreshTime = 0;
  unsigned long lastSubscribeTime = 0;

  while (true) {

    mesh.update();
    client.loop();

    for (auto & pin: pins) {
        if (!(pin.id > 0)) {
            continue;
        }
        if (millis() - pin.lastRead > pin.readInterval) {
            pin.lastRead = millis();
            sendStateData(client, valueProviderFactory, pin);
            wdt_reset();
        } 
    }

	if (millis() - lastRefreshTime >= DISPLAY_TIME) {

        Serial << F("freeMemory ") << freeMemory() << endl;
		    lastRefreshTime += DISPLAY_TIME;

        sendLiveData(client);

        if (reconnect(mesh)) {
		    lastSubscribeTime = 0;
        }

        Serial << F("Ping") << endl;
	}

    if (millis() > lastSubscribeTime)  {
        lastSubscribeTime = millis() + subscribeToChannels(client, subscribeHandler, jsonHandler);
    }

    wdt_reset();

  }
}
