# Moniteth
Moniteth (Monitor Ethernet) is an embedded client/server made to be used between an Adruino ethernet shield
and a linux server. No TCP/IP stack needed, only pure ethernet frames.

# Supported Hardware
- Arduino UNO
- Ethernet Shield W5100

# Quick Setup
This project allows simple communication between your Arduino board and your linux server, using Ethernet.
This comes with some limitation:
- Frame cannot be larger than `1486 bytes`
- You need to hardcode the device identifier per device

But, basicly, that's all.

# Specification
Arduino device will push data to the linux server, using ethernet frame. The linux server runs the gateway
program which translate the payload into a HTTP request to forward to a higher level.

The Ethernet frame type is set to `0x42F0`, any frame using this ethernet type will be read by the gateway.

Arduino MAC Address will follow this prefix: `0A:42:42:42:42`, and the last byte will be used to identify
the device (you can go up to ~250 devices like this, depends if you exclude some identifiers).

Example: `0A:42:42:42:42:01` will be `Device 1`.

# Protocol
The first byte of the payload is a 1 byte value used to describe the type payload. This value is enumerated
as following:
- `MONITETH_TYPE_PING     0x01`
- `MONITETH_TYPE_PONG     0x02`
- `MONITETH_TYPE_DS18X20  0x03`
- `MONITETH_TYPE_DHT22    0x04`
- `MONITETH_TYPE_POWER    0x05`

## MONITETH_TYPE_PING and MONITETH_TYPE_PONG
Not used now

## MONITETH_TYPE_DS18X20
Get a temperature from a DS18x20 sensor.

Payload contains:
- 8 bytes for device id
- 4 bytes for temperature (signed integer)

Temperature needs to be divided by 1000 to have Celsius temperature.

## MONITETH_TYPE_DHT22
Get temperature and humidity from a DHT22 sensor.

Payload contains:
- 2 bytes for device identifier
- 4 bytes signed integer for temperature
- 4 bytes for humidity value in percent

Temperature needs to be divided by 1000 to have Celsius temperature.

## MONITETH_TYPE_POWER
Get the power usage un watt, from current transformer sensor.

Payload contains:
- 1 byte for the phase id
- 4 byte for signed power usage in watt
