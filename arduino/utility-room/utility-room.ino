#include "OneWire.h"
#include "DallasTemperature.h"
#include "DHT.h"
#include "w5100.h"
#include "EmonLib.h"

// laptix bond0 mac address
const byte srvaddr[] = {
    0x28, 0xf1, 0x0e, 0x01, 0x6b, 0x84
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
DeviceAddress sensoraddr;

struct moth_ds18_t {
    uint8_t deviceid[8];
    int32_t temperature;

} __attribute__((packed));

struct moth_dht22_t {
    uint16_t deviceid;
    int32_t temperature;
    int32_t humidity;

} __attribute__((packed));

struct moth_power_t {
    uint8_t phase;
    int32_t power;

} __attribute__((packed));


// dht sensor
#define DHTPIN  7
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// power sensors
#define PWR_SENSORS 3

EnergyMonitor emons[PWR_SENSORS];

// our devices
struct moth_ds18_t *dallas = (struct moth_ds18_t *)((uint8_t *) netbuffer + 15);
struct moth_dht22_t *dht22 = (struct moth_dht22_t *)((uint8_t *) netbuffer + 15);
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

    if(!sensors.getAddress(sensoraddr, 0))
        Serial.println("Unable to find address for Device 0");

    // initializing dht
    dht.begin();

    // init power
    for(int i = 0; i < PWR_SENSORS; i++)
    	emons[i].current(i, 60);
} 

void loop(void)  {
    float temp, hum;
    int32_t convert;

    //
    // ds18b20
    //
    memset(netbuffer + 14, 0x00, sizeof(netbuffer) - 14);

    Serial.println("[+] requesting temperatures"); 
    sensors.requestTemperatures();

    temp = sensors.getTempC(sensoraddr);
    Serial.print("[+] temperature: ");
    Serial.println(temp);

    convert = temp * 1000;
    dallas->temperature = __builtin_bswap32(convert);
    memcpy(dallas->deviceid, sensoraddr, 8);

    Serial.println("[+] sending network frame");
    netbuffer[14] = 0x03;
    w5100.sendFrame(netbuffer, netlength);

    //
    // dht22
    //
    memset(netbuffer + 14, 0x00, sizeof(netbuffer) - 14);

    hum = dht.readHumidity();
    temp = dht.readTemperature();

    Serial.print("[+] humidity: ");
    Serial.print(hum);
    Serial.print(" %, temperature: ");
    Serial.println(temp);

    convert = temp * 1000;
    dht22->temperature = __builtin_bswap32(convert);

    convert = hum * 1000;
    dht22->humidity = __builtin_bswap32(convert);
    dht22->deviceid = __builtin_bswap16(1);

    netbuffer[14] = 0x04;
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