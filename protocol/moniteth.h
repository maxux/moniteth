#ifndef MONITETH_PROTOCOL_H
    #define MONITETH_PROTOCOL_H

    typedef enum moth_type_t {
        MONITETH_TYPE_PING = 0x01,
        MONITETH_TYPE_PONG = 0x02,
        MONITETH_TYPE_DS18X20 = 0x03,
        MONITETH_TYPE_DHT22 = 0x04,
        MONITETH_TYPE_POWER = 0x05,
        MONITETH_TYPE_DS18X20_R1 = 0x06,
        MONITETH_TYPE_SWITCH = 0x07,

    } moniteth_type_t;

    typedef struct moth_ds18_t {
        uint8_t deviceid[8];
        int32_t temperature;

    } moth_ds18_t;

    typedef struct moth_dht22_t {
        uint16_t deviceid;
        int32_t temperature;
        int32_t humidity;

    } moth_dht22_t;

    typedef struct moth_power_t {
        uint8_t phase;
        int32_t power;

    } moth_power_t;

    typedef struct moth_switch_t {
        uint8_t id;

    } moth_switch_t;
#endif
