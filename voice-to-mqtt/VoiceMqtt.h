struct VoiceMqtt
{
    char topic[MAX_TOPIC] {0};
    char message[MAX_MESSAGE] {0};

    static VoiceMqtt fromFlashString(const char * topic, const char * message)
    {
        VoiceMqtt self;
        strncpy_P(self.topic, topic, MIN(sizeof(self.topic) - 1, strlen_P(topic)));
        strncpy_P(self.message, message, MIN(sizeof(self.message) - 1, strlen_P(message)));
        return self;
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
            strncpy_P(self.message, DEFAULT_MESSAGE, MIN(sizeof(self.message) - 1, strlen_P(DEFAULT_MESSAGE)));
        }
        return self;
    }
};