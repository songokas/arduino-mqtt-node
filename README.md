# What is it ?

This repository contains mqtt nodes for different platforms
that can be controlled by sending mqtt messages

e.g.

sending to topic "heating/nodes/123/set/digital/5" message "1" would turn
digital 5 pin on

## Directory structure

* nrf24l01-arduino-node - arduino connecting to mqtt through gateway
* nrf24l01-mqtt-gateway - forwards messages between nrf24l01 and mqtt server
* wifi-esp-nod - connects directly to mqtt server
* src/CustomerProviders - example of custom providers (link it inside once of the previous folder to load it)

## Topics

by default nodes subscribe to the following topics

{NODE_NAME}/set/json - expects json such as {"pin": 13, "set": 1}

{NODE_NAME}/subscribe - expects a new topic to subscribe to


## Howto build

* git clone --recurse-submodules -j8 git://github.com/songokas/arduino-mqtt-node
* build esp8266 related tools `(cd libs/esp8266/tools && python get.py)` or specify in Makefile

### nrf24l01-arduino-node

requires mqtt gateway running

build arduino node with nrf24l01

```
cd nrf24l01-arduino-node
cp Makefile-standalone Makefile
# modify Makefile and provide your NODE_ID NODE_NAME etc.
make && make upload && make monitor
```

tested on arduino nano

### nrf24l01-mqtt-gateway


```
cd nrf24l01-mqtt-gateway
cp Makefile-esp Makefile
# modify Makefile if necessary
cp platform.local.example.txt platform.local.txt
# modify platform.local.txt provide your own keys, change settings
make && make upload && make monitor
```

tested on node-mcu


### wifi-esp-node

connects directly to the mqtt server

```
cd wifi-esp-node
cp Makefile-esp Makefile
# modify Makefile if necessary
cp platform.local.example.txt platform.local.txt
# modify platform.local.txt provide your own keys, change settings
make && make upload && make monitor
```

tested on node-mcu

### voice-to-mqtt

define commands in main.cpp or create custom-commands.h file with content

commands[0] = VoiceMqtt::fromFlashString(PSTR("cmnd/heating/power1"), PSTR("toggle"));

```
cd voice-to-mqtt
cp Makefile-esp Makefile
# modify Makefile if necessary
cp platform.local.example.txt platform.local.txt
# modify platform.local.txt provide your own keys, change settings
make && make upload && make monitor
```

override topic, message for record:

```
mosquitto_pub -h servas -t voice/train/signature/0 -m "cmnd/heating/power1 toggle"
```

#### TODO 

* training does not work on esp8266 devices (only topic and message can be overriden)