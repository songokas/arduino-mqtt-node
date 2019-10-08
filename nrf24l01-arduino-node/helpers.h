void operator delete(void* obj, unsigned int n) { 
    free(obj); 
} 

bool reconnect(RF24Mesh & mesh)
{
    if (!mesh.checkConnection() ) {
        Serial << F("Renewing Mesh Address") << endl;
        if(!mesh.renewAddress(MESH_TIMEOUT)){
            return mesh.begin(RADIO_CHANNEL, RF24_250KBPS, MESH_TIMEOUT);
        } else {
            return true;
        }
    } else {
        auto currentAddress = mesh.getAddress(mesh.getNodeID());
        if (!(currentAddress > 0)) {
            Serial << F("Renew address: ") << currentAddress << endl;
            return mesh.renewAddress(MESH_TIMEOUT);
        }
    }
    return false;
}

bool sendLiveData(MeshMqttClient & client)
{
    char topic[MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), CHANNEL_KEEP_ALIVE);
    if (!client.publish(topic, millis())) {
        Serial << (F("Failed to publish keep alive")) << endl;
        return false;
    }	
    return true;
}

bool sendStateData(MeshMqttClient & client, ValueProviderFactory & provider, const Pin & pin)
{
    MqttMessage msg;
    snprintf_P(msg.topic, COUNT_OF(msg.topic), CHANNEL_INFO, provider.getMatchingTopicType(pin), pin.id);
    provider.formatMessage(msg.message, COUNT_OF(msg.message), pin);

    if (!client.publish(msg)) {
        Serial << (F("Failed to publish state")) << endl;
        return false;
    }
    return true;
}

unsigned long subscribeToChannels(MeshMqttClient & client, SubscribeHandler & subscribeHandler, PinStateJsonHandler & jsonHandler)
{
    unsigned long nextSubscribeIn = 5000;
    char topic[MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), CHANNEL_SUBSCRIBE);
    if (client.subscribe(topic, &subscribeHandler)) {
        Serial << (F("Subscribed for channel: ")) << topic << endl;
        char topic[MAX_LEN_TOPIC] {0};
        snprintf_P(topic, COUNT_OF(topic), CHANNEL_SET_JSON);
        if (client.subscribe(topic, &jsonHandler)) {
            Serial << (F("Subscribed for channel: ")) << topic << endl;
            nextSubscribeIn = (24ul * 3600 * 1000);
        } else {
            Serial << (F("Failed to subscribe: ")) << topic << endl;
        }
    } else {
        Serial << (F("Failed to subscribe: ")) << topic << endl;
    }
    return nextSubscribeIn;
}
