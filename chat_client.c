#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#define SERVER "10.115.12.240"
#define PORT 49153
#define BUFSIZE 1024

void recvandprint(int fd) {
    char buff[BUFSIZE + 1];
    int ret;
    while ((ret = recv(fd, buff, BUFSIZE, 0)) > 0) {
        buff[ret] = '\0';
        printf("%s", buff);
    }
    if (ret == -1 && errno != EAGAIN) {
        perror("recv error");
        exit(errno);
    }
}

void sendout(int fd, char *msg) {
    if (send(fd, msg, strlen(msg), 0) == -1) {
        perror("send error");
        exit(errno);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: chat-client <screenname>\n");
        return 1;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket error");
        return errno;
    }

    struct sockaddr_in sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    if (inet_pton(AF_INET, SERVER, &sin.sin_addr) <= 0) {
        perror("inet_pton error");
        return errno;
    }
    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        perror("connect error");
        return errno;
    }

    struct timeval timev = {0, 500000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timev, sizeof(timev));

    char name_msg[BUFSIZE];
    snprintf(name_msg, sizeof(name_msg), "%s\n", argv[1]);
    sendout(fd, name_msg);

    char *line = NULL;
    size_t len = 0;
    while (1) {
        recvandprint(fd);
        if (getline(&line, &len, stdin) > 1) {
            sendout(fd, line);
            if (strcmp(line, "quit\n") == 0) break;
        }
    }
    free(line);
    close(fd);
    return 0;
}
