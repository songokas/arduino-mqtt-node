

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

void printSigTrain(uint8_t * buf, uint8_t len)
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

uint8_t train(VR &voiceRecognition, uint8_t index, const char * signature)
{
    uint8_t buf[MAX_RECOGNIZED_BUFFER]{0};
    //@TODO bug on esp below
    uint8_t ret = 0;//voiceRecognition.trainWithSignature(index, signature, sizeof(buf), buf);
    if (ret >= 0) {
        printSigTrain(buf, ret);
    } else {
        error("Train with signature failed or timeout.");
    }
    return ret;
}

/**
0x40101014: malloc(size_t) at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/umm_malloc/umm_malloc.cpp line 552
0x40204a5b: std::_Function_base::_Base_manager ::_M_manager(std::_Any_data &, const std::_Any_data &, std::_Manager_operation) at /directory/arduino-mqtt-node/libs/esp8266/tools/xtensa-lx106-elf/xtensa-lx106-elf/include/c++/4.8.2/functional line 1954
0x402106a4: operator new(unsigned int) at /workdir/repo/gcc/libstdc++-v3/libsupc++/new_op.cc line 52
0x40205294: SoftwareSerial::rxBits() at /directory/arduino-mqtt-node/libs/esp8266/tools/xtensa-lx106-elf/xtensa-lx106-elf/include/c++/4.8.2/functional line 1987
0x4010064a: millis() at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/core_esp8266_wiring.cpp line 188
0x40205343: SoftwareSerial::read() at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/libraries/SoftwareSerial/src/SoftwareSerial.cpp line 186
0x4020570d: VR::receive(unsigned char*, int, unsigned short) at /directory/arduino-mqtt-node/arduino-link/VoiceRecognitionV3/VoiceRecognitionV3.cpp line 1232
0x4010064a: millis() at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/core_esp8266_wiring.cpp line 188
0x40205761: VR::receive_pkt(unsigned char*, unsigned short) at /directory/arduino-mqtt-node/arduino-link/VoiceRecognitionV3/VoiceRecognitionV3.cpp line 1196
0x40205888: VR::trainWithSignature(unsigned char, void const*, unsigned char, unsigned char*) at /directory/arduino-mqtt-node/arduino-link/VoiceRecognitionV3/VoiceRecognitionV3.cpp line 199
0x40205ce8: HardwareSerial::write(unsigned char const*, unsigned int) at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/HardwareSerial.h line 164
0x40205cf4: HardwareSerial::write(unsigned char const*, unsigned int) at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/HardwareSerial.h line 165
0x40205ce8: HardwareSerial::write(unsigned char const*, unsigned int) at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/HardwareSerial.h line 164
0x4020199b: train(VR&, unsigned char, char const*) at /directory/arduino-mqtt-node/voice-to-mqtt/build-esp32/sketch/helpers.h line 86
0x4020c44c: strtol at /home/earle/src/esp-quick-toolchain/repo/newlib/newlib/libc/stdlib/strtol.c line 224
0x40201b9a: std::_Function_handler ::_M_invoke(const std::_Any_data &, char *, unsigned char *, unsigned int) at /directory/arduino-mqtt-node/voice-to-mqtt/main.cpp line 134
0x40204238: PubSubClient::readByte(unsigned char*) at /directory/arduino-mqtt-node/arduino-link/PubSubClient/src/PubSubClient.cpp line 231
0x40204373: PubSubClient::readPacket(unsigned char*) at /directory/arduino-mqtt-node/arduino-link/PubSubClient/src/PubSubClient.cpp line 283
0x40204974: PubSubClient::loop() at /directory/arduino-mqtt-node/libs/esp8266/tools/xtensa-lx106-elf/xtensa-lx106-elf/include/c++/4.8.2/functional line 2464
0x40202042: loop() at /directory/arduino-mqtt-node/voice-to-mqtt/main.cpp line 156
0x4010055c: ets_post(uint8, ETSSignal, ETSParam) at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/core_esp8266_main.cpp line 177
0x402069d0: loop_wrapper() at /directory/arduino-mqtt-node/arduino-link/packages/esp8266/hardware/esp8266/2.5.2/cores/esp8266/core_esp8266_main.cpp line 197
**/