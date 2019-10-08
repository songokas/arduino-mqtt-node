void reconnect() {
    // Loop until we're reconnected
    if (!client.connected()) {
        DPRINTLN(F("Attempting MQTT connection..."));
        // Attempt to connect
        if (client.connect("ESP8266Client")) {
            DPRINTLN(F("Mqtt connected"));
        } else {
            Serial << F("Mqtt failed, rc=") << client.state() << F(" try again in 5 seconds") << endl;
            delay(400);
        }
    }
}

bool sendLiveData(PubSubClient & client)
{
    char topic[MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), CHANNEL_KEEP_ALIVE);
    char liveMsg[16] {0};
    sprintf(liveMsg, "%d", millis());
    if (!client.publish(topic, liveMsg)) {
        Serial << (F("Failed to publish keep alive")) << endl;
        return false;
    }	
    return true;
}

bool sendStateData(PubSubClient & client, ValueProviderFactory & provider, const Pin & pin)
{
    char topic[MAX_LEN_TOPIC] {0};
    char message[MAX_LEN_MESSAGE] {0};

    snprintf_P(topic, COUNT_OF(topic), CHANNEL_INFO, provider.getMatchingTopicType(pin), pin.id);
    provider.formatMessage(message, COUNT_OF(message), pin);

    if (!client.publish(topic, message)) {
        Serial << (F("Failed to publish state")) << endl;
        return false;
    }
    return true;
}

void subscribeToChannels(
    PubSubClient & client,
    SubscriberList & subscribers,
    SubscribePubSubHandler & subscribeHandler,
    PinStateJsonHandler & jsonHandler
)
{
    char topic[MAX_LEN_TOPIC] {0};

    snprintf_P(topic, COUNT_OF(topic), CHANNEL_SUBSCRIBE);
    subscribers.add(topic, &subscribeHandler, (uint16_t)0);
    client.subscribe(topic);

    snprintf_P(topic, COUNT_OF(topic), CHANNEL_SET_JSON);
    subscribers.add(topic, &jsonHandler, (uint16_t)0);
    client.subscribe(topic);
}
