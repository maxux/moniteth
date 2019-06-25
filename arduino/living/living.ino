#include "OneWire.h"
#include "DallasTemperature.h"
#include "w5100.h"

// routinx lan mac address
const byte srvaddr[] = {
    0x34, 0x97, 0xf6, 0x3f, 0x99, 0x97
};

// custom device mac address
// last byte will be set by deviceid
byte macaddr[] = {
    0xA2, 0x42, 0x42, 0x42, 0x42, 0x00
};

const uint8_t netdevid = 0x04;
Wiznet5100 w5100;
uint8_t netbuffer[512];
uint16_t netlength = 32;

// one wire stack
#define ONE_WIRE_BUS 2 

#define SENSORS_DEVICES 16

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
DeviceAddress sensoraddr[SENSORS_DEVICES];

struct moth_ds18_t {
    uint8_t deviceid[8];
    int32_t temperature;
};

struct moth_ds18_t device[SENSORS_DEVICES]; // = (struct moth_ds18_t *)((uint8_t *) netbuffer + 14);
int devicesfound = 0;

void setup(void)  { 
    Serial.begin(9600); 

    Serial.println("[+] initializing network");
    
    macaddr[5] = netdevid;

    Serial.println("[+] initializing device");
    Serial.print("[+] device id: ");
    Serial.println(netdevid);

    // destination mac address
    memcpy(netbuffer, srvaddr, 6);

    // source mac address
    memcpy(netbuffer + 6, macaddr, 6);

    // ethernet type
    netbuffer[12] = 0x42;
    netbuffer[13] = 0xF0;

    // init network
    w5100.begin(macaddr);

    Serial.println("[+] initializing sensors");
    sensors.begin(); 
    devicesfound = sensors.getDeviceCount();

    Serial.print("[+] ");
    Serial.print(devicesfound, DEC);
    Serial.println(" devices found");

    for(int i = 0; i < devicesfound; i++) {
        if(!sensors.getAddress(sensoraddr[i], i)) {
            Serial.print("Unable to find address for Device");
            Serial.println(i, DEC);
        }

        memcpy(device[i].deviceid, &sensoraddr[i], 8); 
    }
    
    // packet type (MONITETH_TYPE_DS18X20)
    netbuffer[14] = 0x03;
} 

void loop(void)  { 
    Serial.println("[+] requesting temperatures"); 
    sensors.requestTemperatures();

    for(int i = 0; i < devicesfound; i++) {
        float temp = sensors.getTempC(sensoraddr[i]);
        Serial.print("[+] temperature: ");
        Serial.println(temp);

        if(temp > 55) {
            Serial.println("[-] discarding value");
            continue;
        }
        
        int32_t convert = temp * 1000;
        device[i].temperature = __builtin_bswap32(convert);

        memcpy(netbuffer + 15, &device[i], sizeof(struct moth_ds18_t));

        Serial.println("[+] sending network frame");
        w5100.sendFrame(netbuffer, netlength);
    }

    delay(5000); 
} 
