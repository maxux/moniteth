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
#include <linux/if.h>
#include <linux/if_packet.h>

uint16_t ethtype = 0x42F0;
int ifindex = -1;

void warnp(char *str) {
    fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
}

void diep(char *str) {
    warnp(str);
    exit(EXIT_FAILURE);
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

    unsigned char *x = (unsigned char *) ifr.ifr_hwaddr.sa_data;
    printf("[+] interface address: %02x:%02x:%02x:%02x:%02x:%02x\n" , x[0], x[1], x[2], x[3], x[4], x[5]);

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

        fulldump(buffer, length);
    }

    return 0;
}

