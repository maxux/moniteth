#include "DHT.h"
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

const uint8_t netdevid = 0x02;
Wiznet5100 w5100;
uint8_t netbuffer[512];
uint16_t netlength = 32;

struct moth_dht22_t {
    uint16_t deviceid;
    int32_t temperature;
    int32_t humidity;

} __attribute__((packed));

// dht sensor
#define DHTPIN  7
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// our devices
struct moth_dht22_t *dht22 = (struct moth_dht22_t *)((uint8_t *) netbuffer + 15);

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

    // initializing dht
    dht.begin();
} 

void loop(void)  {
    float temp, hum;
    int32_t convert;

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
    dht22->deviceid = __builtin_bswap16(2);        // device 2

    netbuffer[14] = 0x04;
    w5100.sendFrame(netbuffer, netlength);

    delay(2000);
} 
