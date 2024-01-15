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
#include <sys/epoll.h>
#include <syslog.h>

#define MAXEVENTS 64

static char strbuf[24];
static int yes = 1;

uint16_t ethtype = 0x42F1;
int ifindex = -1;

int warnp(char *str) {
    fprintf(stderr, "[-] %s: %s\n", str, strerror(errno));
    return 1;
}

void diep(char *str) {
    warnp(str);
    exit(EXIT_FAILURE);
}

void dieg(char *str, int status) {
    fprintf(stderr, "[-] %s: %s\n", str, gai_strerror(status));
    exit(EXIT_FAILURE);
}

static char *bufmac(uint8_t *source) {
    ssize_t offset = 0;

    for(int i = 0; i < 6; i++)
        offset += sprintf(strbuf + offset, "%02x:", source[i]);

    strbuf[17] = '\0';

    return strbuf;
}


static int socket_tcp_init(char *listenaddr, char *port) {
    struct addrinfo hints;
    struct addrinfo *sinfo;
    int status;
    int fd;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    printf("[+] network: looking for: host: %s, port: %s\n", listenaddr, port);

    if((status = getaddrinfo(listenaddr, port, &hints, &sinfo)) != 0)
        dieg("getaddrinfo", status);

    if((fd = socket(sinfo->ai_family, SOCK_STREAM, 0)) == -1)
        diep("tcp socket");

    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        diep("tcp setsockopt");

    if(bind(fd, sinfo->ai_addr, sinfo->ai_addrlen) == -1)
        diep("tcp bind");

    if(listen(fd, SOMAXCONN) == -1)
        diep("listen");

    freeaddrinfo(sinfo);

    return fd;
}

static int socket_raw_init(char *interface) {
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

static void sendframe(int sockfd, const uint8_t *data, uint16_t length) {
    struct sockaddr_ll saddr;

    saddr.sll_ifindex = ifindex;
    saddr.sll_halen = ETH_ALEN;

    if(sendto(sockfd, data, length, 0, (struct sockaddr *) &saddr, sizeof(struct sockaddr_ll)) < 0)
        warnp("sendto");
}

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

void open_door(int sockfd, char *door) {
    uint8_t buffer[1500];
    uint8_t source[] = {0x34, 0x97, 0xf6, 0x3f, 0x99, 0x97};
    uint8_t destination[] = {0xA2, 0x42, 0x42, 0x42, 0x42, 0xA0};

    memset(buffer, 0x00, sizeof(buffer));

    memcpy(buffer, destination, sizeof(destination));
    memcpy(buffer + 6, source, sizeof(source));
    buffer[12] = 0x42;
    buffer[13] = 0xF1;

    char x[64];
    sprintf(x, "OPEN DOOR %s ---", door);
    memcpy(buffer + 14, x, strlen(x));

    int length = 14 + strlen(x);

    printf("[+] sending open door frame\n");
    sendframe(sockfd, buffer, length);
    fulldump(buffer, length);
}

void handle_door_socket(int sockfd) {
    uint8_t buffer[1500];
    uint16_t length;

    if((length = readframe(sockfd, buffer, sizeof(buffer))) <= 0)
        return;

    // reject unknown source
    if(validate(buffer + 6)) {
        printf("[-] unauthorized frame from: %s\n", bufmac(buffer + 6));
        return;
    }

    // fulldump(buffer, length);

    uint8_t source[6];
    memcpy(source, buffer + 6, 6);

    char *ping = "HEY I'M ALIVE";
    if(memcmp(buffer + 14, ping, strlen(ping)) == 0) {
        char *mac = bufmac(source);
        printf("[+] ping message from device: %s\n", mac);
    }
}

int handle_trigger_socket(int sockfd) {
    int clientfd;
    char buffer[1500];
    int length;
    int status = 0;

    printf("[+] trigger: accpting new connection request\n");
    if((clientfd = accept(sockfd, NULL, NULL)) < 0) {
        perror("accept");
        return 0;
    }

    printf("[+] trigger: waiting message\n");
    if((length = recv(clientfd, buffer, sizeof(buffer), 0)) < 0) {
        perror("recv");
        return 0;
    }

    buffer[length] = '\0';
    printf("[+] trigger: message length: %d, %s\n", length, buffer);

    char *matchl = "OPEN DOOR LEFT PLEASE";
    char *matchr = "OPEN DOOR RIGHT PLEASE";

    if(strncmp(buffer, matchl, strlen(matchl)) == 0) {
        printf("[+] trigger: open [left] door message received\n");
        status = 1;

    } else if(strncmp(buffer, matchr, strlen(matchr)) == 0) {
        printf("[+] trigger: open [right] door message received\n");
        status = 2;

    } else {
        printf("[-] trigger client: invalid message\n");
    }

    close(clientfd);

    return status;
}

int main(int argc, char *argv[]) {
    char *intf = "lan";
    int evfd;

    printf("[+] initializing garage door gateway\n");
    openlog("doorgw", LOG_CONS | LOG_PID, LOG_LOCAL7);
    syslog(LOG_INFO, "Initializing garage gateway");

    if(argc > 1)
        intf = argv[1];

    printf("[+] listening on interface: %s\n", intf);

    // create socket
    int doorfd = socket_raw_init(intf);
    int triggerfd = socket_tcp_init("10.241.0.254", "47587");

    // epoll
    struct epoll_event event;
    struct epoll_event *events = NULL;

    memset(&event, 0, sizeof(struct epoll_event));

    if((evfd = epoll_create1(0)) < 0)
        diep("epoll_create1");

    event.events = EPOLLIN;
    event.data.fd = doorfd;

    if(epoll_ctl(evfd, EPOLL_CTL_ADD, doorfd, &event) < 0)
        diep("epoll_ctl: door");

    event.data.fd = triggerfd;

    if(epoll_ctl(evfd, EPOLL_CTL_ADD, triggerfd, &event) < 0)
        diep("epoll_ctl: trigger");

    events = calloc(MAXEVENTS, sizeof event);

    while(1) {
        printf("[+] waiting for network activity\n");
        int n = epoll_wait(evfd, events, MAXEVENTS, -1);
        if(n < 0)
            diep("epoll_wait");

        for(int i = 0; i < n; i++) {
            struct epoll_event *ev = events + i;

            if(ev->data.fd == doorfd) {
                printf("[+] network activity: door socket\n");
                handle_door_socket(doorfd);
            }

            if(ev->data.fd == triggerfd) {
                printf("[+] network activity: trigger socket\n");
                int status = handle_trigger_socket(triggerfd);

                if(status == 1) {
                    printf("[+] trigger open door (left)\n");
                    syslog(LOG_INFO, "Trigger garage door button [left]");
                    open_door(doorfd, "LEFT");
                }

                if(status == 2) {
                    printf("[+] trigger open door (right)\n");
                    syslog(LOG_INFO, "Trigger garage door button [right]");
                    open_door(doorfd, "RIGHT");
                }

            }
        }
    }

    return 0;
}

