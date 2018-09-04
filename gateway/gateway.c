#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/ether.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <linux/if.h>
#include <linux/if_packet.h>
#include "../protocol/moniteth.h"

#define SERVER_TARGET  "10.241.0.254"
#define SERVER_PORT    30502

time_t cooldown[2048] = {0};

static char strbuf[24];

uint16_t ethtype = 0x42F0;
int ifindex = -1;

int warnp(char *str) {
    fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
    return 1;
}

void diep(char *str) {
    warnp(str);
    exit(EXIT_FAILURE);
}

static char *bufmac(uint8_t *source) {
    ssize_t offset = 0;

    for(int i = 0; i < 6; i++)
        offset += sprintf(strbuf + offset, "%02x:", source[i]);

    strbuf[17] = '\0';

    return strbuf;
}

static char *ds18id(uint8_t *buffer) {
    size_t offset = 3;

    sprintf(strbuf, "%02x-", buffer[0]);

    for(int i = 1; i < 8; i++)
        offset += sprintf(strbuf + offset, "%02x", buffer[i]);

    return strbuf;
}

static char *dhtid(uint16_t id) {
    sprintf(strbuf, "d-%d", id);
    return strbuf;
}


// BAAAAAAAAAAAAAAAD.
int http(char *endpoint, char *argv1, char *argv2, char *argv3, int dirty) {
    int sockfd;
    struct sockaddr_in addr_remote;
    struct hostent *hent;
    char payload[512];

    addr_remote.sin_family = AF_INET;
    addr_remote.sin_port = htons(SERVER_PORT);

    if((hent = gethostbyname(SERVER_TARGET)) == NULL)
        return warnp("http: gethostbyname");

    memcpy(&addr_remote.sin_addr, hent->h_addr_list[0], hent->h_length);

    if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        return warnp("http: socket");

    if(connect(sockfd, (const struct sockaddr *) &addr_remote, sizeof(addr_remote)) < 0)
        return warnp("http: connect");

    if(dirty == 1) {
        sprintf(payload, "GET /%s/%ld/%s/%s HTTP/1.0\r\n\r\n", endpoint, time(NULL), argv1, argv2);

    } else if(dirty == 2) {
        sprintf(payload, "GET /%s/%s/%ld/%s HTTP/1.0\r\n\r\n", endpoint, argv1, time(NULL), argv2);

    } else if(dirty == 3) {
        sprintf(payload, "GET /%s/%s/%ld/%s/%s HTTP/1.0\r\n\r\n", endpoint, argv1, time(NULL), argv2, argv3);
    }

    if(send(sockfd, payload, strlen(payload), 0) < 0)
        return warnp("http send");

    close(sockfd);

    return 0;
}

int http_power(moth_power_t *power) {
    char argv1[128];
    char argv2[128];
    int cid = power->phase + 40;

    if(cooldown[cid] > time(NULL))
        return 1;

    sprintf(argv1, "%d", power->phase);
    sprintf(argv2, "%d", power->power);

    cooldown[cid] = time(NULL) + 2;

    return http("power", argv1, argv2, NULL, 1);
}

int http_ds18(moth_ds18_t *sensor) {
    char argv1[128];
    char argv2[128];

    int id = 500 + sensor->deviceid[7];

    if(cooldown[id] > time(NULL))
        return 1;

    sprintf(argv1, "%s", ds18id(sensor->deviceid));
    sprintf(argv2, "%.2f", sensor->temperature / 1000.0);

    cooldown[id] = time(NULL) + 60;

    return http("sensors", argv1, argv2, NULL, 2);
}

int http_dht(moth_dht22_t *sensor) {
    char argv1[128];
    char argv2[128];
    char argv3[128];
    int did = sensor->deviceid + 100;

    if(cooldown[did] > time(NULL))
        return 1;

    sprintf(argv1, "%s", dhtid(sensor->deviceid));
    sprintf(argv2, "%.2f", sensor->temperature / 1000.0);
    sprintf(argv3, "%.2f", sensor->humidity / 1000.0);

    cooldown[did] = time(NULL) + 60;

    return http("sensors-dht", argv1, argv2, argv3, 3);
}

static int socket_init(char *interface) {
    int sockfd;
    struct ifreq ifr;

    if((ifindex = if_nametoindex(interface)) <= 0)
        diep("if_nametoindex");

    if((sockfd = socket(PF_PACKET, SOCK_RAW, htons(ethtype))) < 0)
        diep("socket");

    // get interface mac address
    ifr.ifr_addr.sa_family = AF_INET;
    strncpy(ifr.ifr_name , interface , IFNAMSIZ - 1);
    ioctl(sockfd, SIOCGIFHWADDR, &ifr);

    printf("[+] interface address: %s\n" , bufmac((uint8_t *) ifr.ifr_hwaddr.sa_data));

    if(setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, interface, IFNAMSIZ-1) < 0)
        diep("reuse: setsockopt");

    return sockfd;
}

void fulldump(void *_data, size_t len) {
    uint8_t *data = _data;
    unsigned int i, j;

    printf("[*] data fulldump [%p -> %p] (%lu bytes)\n", data, data + len, len);
    printf("[*] 0x0000: ");

    for(i = 0; i < len; ) {
        printf("%02x ", data[i++]);

        if(i % 16 == 0 || i == len) {
            if(i % 16) {
                // skip rest of the line not printed
                printf("%*s", (16 - (i % 16)) * 3, " ");
            }

            printf("|");

            for(j = i - 16; j < i; j++)
                printf("%c", ((isprint(data[j]) ? data[j] : '.')));

            printf("|\n");

            if(i % 16 == 0)
                printf("[*] 0x%04x: ", i);
        }
    }

    printf("\n");
}

/*
static void sendframe(int sockfd, const uint8_t *data, uint16_t length) {
    struct sockaddr_ll saddr;

    saddr.sll_ifindex = ifindex;
    saddr.sll_halen = ETH_ALEN;

    if(sendto(sockfd, data, length, 0, (struct sockaddr *) &saddr, sizeof(struct sockaddr_ll)) < 0)
        warnp("sendto");
}
*/

int validate(uint8_t *source) {
    // only accept mac address following A2:42:42:42:42:xx
    uint8_t check[5] = {0xa2, 0x42, 0x42, 0x42, 0x42};
    if(memcmp(source, check, 5))
        return 1;

    printf("[+] authorized frame from: %s\n", bufmac(source));
    return 0;
}

static uint16_t readframe(int sockfd, uint8_t *buffer, uint16_t bufsize) {
    int length;

    if((length = recv(sockfd, buffer, bufsize, 0)) < 0) {
        if(errno == EAGAIN)
            return 0;

        diep("recv");
    }

    return length;
}

int main(int argc, char *argv[]) {
    char *intf = "bond0";
    uint8_t buffer[1500];
    uint16_t length;
    int sockfd;

    printf("[+] initializing ethernet gateway\n");

    if(argc > 1)
        intf = argv[1];

    printf("[+] listening on interface: %s\n", intf);

    // create socket
    sockfd = socket_init(intf);

    // listening
    while(1) {
        printf("[+] waiting ethernet frame\n");
        if((length = readframe(sockfd, buffer, sizeof(buffer))) <= 0)
            continue;

        // reject unknown source
        if(validate(buffer + 6)) {
            printf("[-] unauthorized frame from: %s\n", bufmac(buffer + 6));
            continue;
        }

        if(buffer[14] == MONITETH_TYPE_DS18X20) {
            moth_ds18_t dallas;
            uint32_t convert;

            memcpy(dallas.deviceid, buffer + 15, 8);
            memcpy(&convert, buffer + 23, 4);

            dallas.temperature = __builtin_bswap32(convert);

            printf("[+] ds18b20: [%s]: %.2f°C\n", ds18id(dallas.deviceid), dallas.temperature / 1000.0);
            http_ds18(&dallas);
        }

        if(buffer[14] == MONITETH_TYPE_DHT22) {
            moth_dht22_t dht22;
            uint32_t convert32;
            uint16_t convert16;

            memcpy(&convert16, buffer + 15, 2);
            dht22.deviceid = __builtin_bswap16(convert16);

            memcpy(&convert32, buffer + 17, 4);
            dht22.temperature = __builtin_bswap32(convert32);

            memcpy(&convert32, buffer + 21, 4);
            dht22.humidity = __builtin_bswap32(convert32);

            printf("[+] dht22: [%d]: %.2f°C - %.2f %%\n", dht22.deviceid, dht22.temperature / 1000.0, dht22.humidity / 1000.0);
            http_dht(&dht22);
        }

        if(buffer[14] == MONITETH_TYPE_POWER) {
            moth_power_t power;
            uint32_t convert32;

            power.phase = *(buffer + 15);

            memcpy(&convert32, buffer + 16, 4);
            power.power = __builtin_bswap32(convert32);

            printf("[+] power: [phase %d]: %d watt\n", power.phase, power.power);
            http_power(&power);
        }


        // fulldump(buffer + 14, length - 14);
    }

    return 0;
}

