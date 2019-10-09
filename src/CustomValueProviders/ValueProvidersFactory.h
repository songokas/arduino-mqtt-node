OneWire oneWire(3); 
DallasTemperature sensor(&oneWire);

DallasTemperatureProvider temperatureProvider(sensor);

// type 0 = digital, 1 = analog, others can be chosen
// add new provider with id 2 associate temperature provider for pin 3
valueProviderFactory.addByType(2, &temperatureProvider, 3);
