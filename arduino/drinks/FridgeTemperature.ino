#include <OneWire.h>
#include <DallasTemperature.h>
#include <printf.h>
#include <nRF24L01.h>
#include <RF24_config.h>
#include <RF24.h>

#define ONE_WIRE_BUS 2

#define SPI_CE   9
#define SPI_CSN  10

const byte temp_listen_addr[5] = {'M','X','E','T','H'};

RF24 radio(SPI_CE, SPI_CSN);
OneWire oneWire(ONE_WIRE_BUS);  
DallasTemperature sensors(&oneWire);

void setup(void) {
  sensors.begin();
  Serial.begin(9600);

  pinMode(LED_BUILTIN, OUTPUT);

  radio.begin();
  radio.openWritingPipe(temp_listen_addr);
  radio.setPALevel(RF24_PA_MAX);
  radio.stopListening();
}

void sendstr(char *str) {
  digitalWrite(LED_BUILTIN, HIGH);
  
  Serial.print("[+] sending frame: ");
  Serial.print(str);
  
  if(radio.write(str, strlen(str))) {
    Serial.println(" - ACK");
  } else {
    Serial.println(" - NO ACK");
  }
  
  digitalWrite(LED_BUILTIN, LOW);
}

void loop(void) {
  char frame[64];
  
  sensors.requestTemperatures();

  float top = sensors.getTempCByIndex(1);
  int topi = top * 1000;
  Serial.println(top);
  float bottom = sensors.getTempCByIndex(0);
  int bottomi = bottom * 1000;
  Serial.println(bottom);
  
  sprintf(frame, "fridge-drink-top %d", topi);
  sendstr(frame);

  sprintf(frame, "fridge-drink-bottom %d", bottomi);
  sendstr(frame);

  delay(5000);
}
