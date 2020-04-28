bool sendMqttMessage(PubSubClient & client, const MqttMessage & data)
{
    if (client.publish(data.topic, data.message)) {
        error("Failed to publish message. Topic: %s Message: %s", data.topic, data.message);
        return false;
    } 
    debug("Sent %s %s", data.topic, data.message);
    return true;
}

bool forwardToMqtt(EncryptedNetwork & network, PubSubClient & mqttClient)
{
    MqttMessage message;
    RF24NetworkHeader header;
    if (!network.receive(&message, sizeof(message), 0, header)) {
        error("Failed to read message");
        return false;
    }
    return sendMqttMessage(mqttClient, message);
}

bool addToQueue(MessageQueueItem * messageQueue, size_t len, const MqttMessage & message, uint16_t node)
{
    for (size_t i = 0; i < len; i++) {
        MessageQueueItem & item = messageQueue[i];
        if (!item.initialized) {
            item.message = message;
            item.node = node;
            item.initialized = true;
            return true;
        }
    }
    return false;
}

uint8_t sendMessages(EncryptedNetwork & encNetwork, MessageQueueItem * messageQueue, size_t len, uint8_t maxFailures = 60)
{
    uint8_t count = 0;
    for (size_t i = 0; i < len; i++) {
        MessageQueueItem & item = messageQueue[i];
        if (item.initialized == true && item.failedToSend > maxFailures) {
            item = {};
        }
        if (!item.initialized) {
            continue;
        }
        if (!encNetwork.send(&item.message, sizeof(item.message), 0, item.node)) {
            warning("Failed to send data to node: %d %d", item.node, item.failedToSend);
            item.failedToSend++;
        } else {
            count++;
            item = {};
        }
    }
    return count;
}