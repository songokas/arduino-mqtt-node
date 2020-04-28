
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

uint8_t sendMessages(MessageQueueItem * messageQueue, size_t len)
{
    uint8_t count = 0;
    for (size_t i = 0; i < len; i++) {
        MessageQueueItem & item = messageQueue[i];
        if (item.initialized == true && item.failedToSend > MAX_MESSAGE_FAILURES) {
            item = {};
        }
        if (!item.initialized) {
            continue;
        }
        if (!encMesh.send(&item.message, sizeof(item.message), (uint8_t)MessageType::Publish, item.node)) {
            warning("Failed to send data to node: %d %d", item.node, item.failedToSend);
            item.failedToSend++;
        } else {
            count++;
            item = {};
        }
    }
    return count;
}
