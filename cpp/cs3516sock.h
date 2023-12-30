#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

// Set the following port to a unique number:
#define MYPORT 5950

int sock;

void dieWithError(const char *errMsg);

int create_cs3516_socket(in_addr ip)
{
    struct sockaddr_in server = {0};

    sock = socket(AF_INET, SOCK_DGRAM, 0);

    printf("Socket selected: %d\n", sock);

    int y = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &y, sizeof(y)) == -1) {
        dieWithError("Error setting socket option!");
        perror("setsockopt");
        exit(1);
    }

    if (sock < 0)
        dieWithError("Error creating CS3516 socket");

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = ip.s_addr;
    server.sin_port = htons(MYPORT);

    int bound = bind(sock, (struct sockaddr *)&server, sizeof(server));
    if (bound < 0) {

        printf("Failed to bind socket %d\n", bound);
        perror("bind");
        close(sock);
        sleep(30);
        dieWithError("Unable to bind CS3516 socket");
    }

    // Socket is now bound:
    return sock;
}

int cs3516_recv(int sock, char *buffer, int buff_size)
{
    struct sockaddr_in from;
    int fromlen, n;
    fromlen = sizeof(struct sockaddr_in);
    n = recvfrom(sock, buffer, buff_size, 0,
                 (struct sockaddr *)&from, (socklen_t *)&fromlen);

    return n;
}

int cs3516_send(int sock, char *buffer, int buff_size, unsigned long nextIP)
{
    struct sockaddr_in to;
    int tolen, n;

    tolen = sizeof(struct sockaddr_in);

    // Okay, we must populate the to structure.
    bzero(&to, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_port = htons(MYPORT);
    to.sin_addr.s_addr = nextIP;

    // We can now send to this destination:
    n = sendto(sock, buffer, buff_size, 0,
               (struct sockaddr *)&to, tolen);

    return n;
}

void dieWithError(const char *errMsg)
{
    close(sock);
    printf("%s\n", errMsg);
    printf("\n");
    exit(1);
}
