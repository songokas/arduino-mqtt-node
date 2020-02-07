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
    if (!provider.formatMessage(message, COUNT_OF(message), pin)) {
        Serial << (F("Failed to format message")) << endl;
        return false; 
    }

    if (!client.publish(topic, message)) {
        Serial << (F("Failed to publish state")) << endl;
        return false;
    } else {
        Serial << F("Sent ") << topic << F(" ") << message << endl;
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
