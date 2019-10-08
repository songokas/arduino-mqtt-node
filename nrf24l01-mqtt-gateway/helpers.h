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
    Serial.println(F("********Assigned Addresses********"));
     for(int i=0; i<mesh.addrListTop; i++){
       Serial.print(F("NodeID: "));
       Serial.print(mesh.addrList[i].nodeID);
       Serial.print(F(" RF24Network Address: 0"));
       Serial.println(mesh.addrList[i].address,OCT);
     }
    Serial.println(F("**********************************"));
}

bool addToQueue(const MqttMessage & message, uint16_t node)
{
   for (auto & item: messageQueue) {
        if (!item.initialized) {
            item.message = message;
            item.node = node;
            item.initialized = true;
            return true;
        }
    }
   return false;
}

uint8_t sendMessages()
{
    uint8_t count = 0;
    for (auto & item: messageQueue) {
        if (item.initialized == true && item.failedToSend > MAX_MESSAGE_FAILURES) {
            item = {};
        }
        if (!item.initialized) {
            continue;
        }
        if (!encMesh.send(&item.message, sizeof(item.message), (uint8_t)MessageType::Publish, item.node)) {
            DPRINT(F("Failed to send data to node: ")); DPRINTLN(item.node); DPRINTLN(item.failedToSend);
            item.failedToSend++;
        } else {
            count++;
            item = {};
        }
    }
    return count;
}
