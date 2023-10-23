#include "OneWire.h"
#include "DallasTemperature.h"
#include "w5100.h"
#include "EmonLib.h"

// routinx lan mac address
const byte srvaddr[] = {
    0x34, 0x97, 0xf6, 0x3f, 0x99, 0x97
};

// custom device mac address
// last byte will be set by deviceid
byte macaddr[] = {
    0xA2, 0x42, 0x42, 0x42, 0x42, 0x00
};

const uint8_t netdevid = 0x01;
Wiznet5100 w5100;
uint8_t netbuffer[512];
uint16_t netlength = 32;

// one wire stack
#define ONE_WIRE_BUS 2 

OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);
DeviceAddress sensoraddr0, sensoraddr1;

struct moth_ds18_t {
    uint8_t deviceid[8];
    int32_t temperature;

} __attribute__((packed));

struct moth_power_t {
    uint8_t phase;
    int32_t power;

} __attribute__((packed));

// power sensors
#define PWR_SENSORS 3

EnergyMonitor emons[PWR_SENSORS];

// our devices
struct moth_ds18_t *dallas = (struct moth_ds18_t *)((uint8_t *) netbuffer + 15);
struct moth_power_t *power = (struct moth_power_t *)((uint8_t *) netbuffer + 15);

static inline uint16_t __builtin_bswap16(uint16_t a) {
    return (a << 8) | (a >> 8);
}

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

    Serial.print("[+] ");
    Serial.print(sensors.getDeviceCount(), DEC);
    Serial.println(" devices found");

    if(!sensors.getAddress(sensoraddr0, 0))
        Serial.println("Unable to find address for Device 0");

    if(!sensors.getAddress(sensoraddr1, 1))
        Serial.println("Unable to find address for Device 1");

    // init power
    for(int i = 0; i < PWR_SENSORS; i++)
    	emons[i].current(i, 60);
} 

void loop(void)  {
    float temp, hum;
    int32_t convert;

    Serial.println("[+] requesting temperatures"); 
    sensors.requestTemperatures();

    //
    // ds18b20
    //
    memset(netbuffer + 14, 0x00, sizeof(netbuffer) - 14);

    temp = sensors.getTempC(sensoraddr0);
    Serial.print("[+] temperature: ");
    Serial.println(temp);

    convert = temp * 1000;
    dallas->temperature = __builtin_bswap32(convert);
    memcpy(dallas->deviceid, sensoraddr0, 8);

    Serial.println("[+] sending network frame");
    netbuffer[14] = 0x03;
    w5100.sendFrame(netbuffer, netlength);

    //
    //
    //
    memset(netbuffer + 14, 0x00, sizeof(netbuffer) - 14);

    temp = sensors.getTempC(sensoraddr1);
    Serial.print("[+] temperature: ");
    Serial.println(temp);

    convert = temp * 1000;
    dallas->temperature = __builtin_bswap32(convert);
    memcpy(dallas->deviceid, sensoraddr1, 8);

    Serial.println("[+] sending network frame");
    netbuffer[14] = 0x03;
    w5100.sendFrame(netbuffer, netlength);

    //
    // power meter
    //
    memset(netbuffer + 14, 0x00, sizeof(netbuffer) - 14);

    double value;

    for(int i = 0; i < PWR_SENSORS; i++) {
        value = emons[i].calcIrms(1480);
        convert = value * 230.0;

        Serial.print("[+] power: phase ");
        Serial.print(i);
        Serial.print(": ");
        Serial.print(convert);
        Serial.println(" watt");

        power->phase = i;
        power->power = __builtin_bswap32(convert);

        netbuffer[14] = 0x05;
        w5100.sendFrame(netbuffer, netlength);
    }
} 
