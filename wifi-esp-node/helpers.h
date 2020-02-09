bool sendLiveData(PubSubClient & client)
{
    char topic[MAX_LEN_TOPIC] {0};
    snprintf_P(topic, COUNT_OF(topic), CHANNEL_KEEP_ALIVE);
    char liveMsg[16] {0};
    sprintf(liveMsg, "%d", millis());
    if (!client.publish(topic, liveMsg)) {
        Serial << F("Failed to publish keep alive") << endl;
        return false;
    }	
    return true;
}

bool sendPostRequest(HTTPClient & client, ValueProviderFactory & provider, const Pin & pin, JsonDocument & json)
{
    char url[MAX_LEN_URL] {0};
    char message[MAX_LEN_JSON_MESSAGE] {0};

    snprintf_P(url, COUNT_OF(url), CHANNEL_SERVER, provider.getMatchingTopicType(pin), pin.id);
    if (!provider.addJson(json, pin)) {
        Serial << (F("Failed to format message")) << endl;
        return false; 
    }

    if (!(serializeJson(json, message, COUNT_OF(message)) > 0)) {
        Serial << (F("Failed to serialize json")) << endl;
        return false;  
    }
    if (!client.begin(url)) {
        Serial << F("Http connection failed") << endl;
        return false;
    }
    client.addHeader("Content-Type", "application/json"); 
    int httpCode = client.POST((uint8_t*)message, strlen(message));  
    client.end(); 

    if (httpCode != 200) {
        Serial << (F("Failed to publish state")) << endl;
        return false;
    } else {
        Serial << F("Sent ") << url << F(" ") << message << endl;
    }
    return true;
}


bool sendMqttRequest(PubSubClient & client, ValueProviderFactory & provider, const Pin & pin)
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
    if (subscribers.add(topic, &subscribeHandler, (uint16_t)0)) {
        Serial << F("Failed to subscribe to topic: ") << topic << endl;
        return;
    }
    client.subscribe(topic);

    snprintf_P(topic, COUNT_OF(topic), CHANNEL_SET_JSON);
    if (subscribers.add(topic, &jsonHandler, (uint16_t)0)) {
        Serial << F("Failed to subscribe to topic: ") << topic << endl;
        return;
    }
    client.subscribe(topic);
}
