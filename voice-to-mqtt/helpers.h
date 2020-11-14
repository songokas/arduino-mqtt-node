struct VoiceMqtt
{
    char topic[40] {0};
    char message[20] {0};

    static VoiceMqtt fromFlashString(const char * topic, const char * message)
    {
        VoiceMqtt self;
        strncpy_P(self.topic, topic, MIN(sizeof(self.topic) - 1, strlen_P(topic)));
        strncpy_P(self.message, message, MIN(sizeof(self.message) - 1, strlen_P(message)));
    }

    static VoiceMqtt fromPayload(uint8_t * payload, uint16_t len) {
        VoiceMqtt self;
        char fullMessage[sizeof(self.topic) + sizeof(self.message)] {0};
        strncpy(fullMessage, (char*)payload, MIN(sizeof(fullMessage) - 1, len));
        uint8_t pos = findNextPos(fullMessage, 0, ' ');
        if (pos > 0) {
            strncpy(self.topic, fullMessage, MIN(sizeof(self.topic) -1, pos));
            strncpy(self.message, fullMessage+pos+1, MIN(sizeof(self.message) - 1, strlen(fullMessage) - pos));
        } else {
            strncpy(self.topic, fullMessage, MIN(sizeof(self.topic) - 1, len));
            strncpy(self.message, DEFAULT_MESSAGE, MIN(sizeof(self.message) - 1, strlen(DEFAULT_MESSAGE)));
        }
        return self;
    }
};

bool loadGroup(VR & voiceRecognition, uint8_t group)
{
    uint8_t index {group * MAX_LOADED};
    uint8_t loadUntil {index + MAX_LOADED};
    if (voiceRecognition.clear() != 0) {
        error("Failed to clear recognizer.");
        return false;
    } 
    info("Load group %d", group);
    while (index < loadUntil) {
        if(voiceRecognition.load(index) != 0) {
            warning("Failed to load record: %d", index);
        }
        index++;
    }
    return true;
}

void printSigTrain(uint8_t *buf, uint8_t len)
{
    if (len == 0) {
        info("Train With Signature Finish.");
        return;
    } else {
        Serial.print(F("Success: "));
        Serial.println(buf[0], DEC);
    }
    Serial.print(F("Record "));
    Serial.print(buf[1], DEC);
    Serial.print(F("\t"));
    switch (buf[2]) {
    case 0:
        Serial.println(F("Trained"));
        break;
    case 0xF0:
        Serial.println(F("Trained, signature truncate"));
        break;
    case 0xFE:
        Serial.println(F("Train Time Out"));
        break;
    case 0xFF:
        Serial.println(F("Value out of range"));
        break;
    default:
        Serial.print(F("Unknown status "));
        Serial.println(buf[2], HEX);
        break;
    }
    Serial.print(F("SIG: "));
    Serial.write(buf + 3, len - 3);
    Serial.println();
}

int train(VR &voiceRecognition, uint8_t index, const char * signature)
{
    uint8_t buf[MAX_RECOGNIZED_BUFFER];
    int ret = voiceRecognition.trainWithSignature(index, signature, sizeof(buf), buf);
    if (ret >= 0) {
        printSigTrain(buf, ret);
    } else {
        error("Train with signature failed or timeout.");
    }
    return ret;
}