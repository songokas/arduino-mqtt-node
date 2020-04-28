
bool sendPostRequest(HTTPClient & client, ValueProviderFactory & provider, const Pin & pin, JsonDocument & json)
{
    char url[MAX_LEN_URL] {0};
    char message[MAX_LEN_JSON_MESSAGE] {0};

    snprintf_P(url, COUNT_OF(url), CHANNEL_SERVER, provider.getMatchingTopicType(pin), pin.id);
    if (!provider.addJson(json, pin)) {
        error("Failed to format message");
        return false; 
    }

    if (!(serializeJson(json, message, COUNT_OF(message)) > 0)) {
        error("Failed to serialize json");
        return false;  
    }
    if (!client.begin(url)) {
        error("Http connection failed");
        return false;
    }
    client.addHeader("Content-Type", "application/json"); 
    int httpCode = client.POST((uint8_t*)message, strlen(message));  
    client.end(); 

    if (httpCode != 200) {
        error("Failed to publish state");
        return false;
    } else {
        info("Sent %s %s", url, message);
    }
    return true;
}


bool sendMqttRequest(PubSubClient & client, ValueProviderFactory & provider, const Pin & pin)
{
    char topic[MAX_LEN_TOPIC] {0};
    char message[MAX_LEN_MESSAGE] {0};

    snprintf_P(topic, COUNT_OF(topic), CHANNEL_INFO, provider.getMatchingTopicType(pin), pin.id);
    
    if (!provider.formatMessage(message, COUNT_OF(message), pin)) {
        error("Failed to format message");
        return false; 
    }

    if (!client.publish(topic, message)) {
        error("Failed to publish state");
        return false;
    } else {
        info("Sent %s %s", topic, message);
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
        error("Failed to subscribe to topic: %s", topic);
        return;
    }
    client.subscribe(topic);

    snprintf_P(topic, COUNT_OF(topic), CHANNEL_SET_JSON);
    if (subscribers.add(topic, &jsonHandler, (uint16_t)0)) {
        error("Failed to subscribe to topic: %s", topic);
        return;
    }
    client.subscribe(topic);
}
