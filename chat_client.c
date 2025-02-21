// Author: Connor Rogstad
// Last Updated: 02/20/2025

// imports
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

// Server Specifics and Constants
#define SERVER "10.115.12.240"
#define PORT 49153
#define BUFSIZE 1024

// receives data from the server from a socket and stores it up to the buffer size
// this will make sure you receive the messages from other users
void recvandprint(int fd) {
    char buff[BUFSIZE + 1];
    int ret;
    // continuously save messages
    while ((ret = recv(fd, buff, BUFSIZE, 0)) > 0) {
        buff[ret] = '\0';
        printf("%s", buff);
    }
    // error handling (allows EAGAIN - no available data)
    if (ret == -1 && errno != EAGAIN) {
        perror("recv error");
        exit(errno);
    }
}

// sends the messages from the user-client to the server through sockets
void sendout(int fd, char *msg) {
    // will send the msg if no error
    if (send(fd, msg, strlen(msg), 0) == -1) {
        perror("send error");
        exit(errno);
    }
}

// main function where most of the chat client logic is handled
int main(int argc, char *argv[]) {
    // ensures proper amt of arguments are passed in
    if (argc != 2) {
        printf("Usage: chat-client <screenname>\n");
        return 1;
    }

    // create my socket to receive and send data from a server
    // error handling for socket
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        perror("socket error");
        return errno;
    }

    // Sets up and defines the server address
    struct sockaddr_in sin = {0};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(PORT);
    // handles errors with setting up the server
    if (inet_pton(AF_INET, SERVER, &sin.sin_addr) <= 0) {
        perror("inet_pton error");
        return errno;
    }

    // connects to the server! handles errors as well
    if (connect(fd, (struct sockaddr *)&sin, sizeof(sin)) == -1) {
        perror("connect error");
        return errno;
    }

    // sets up a timeout which will error in the case of data not being received within 0.5 seconds
    struct timeval timev = {0, 500000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timev, sizeof(timev));

    // prints out the username of the user entering the chat room
    char name_msg[BUFSIZE];
    snprintf(name_msg, sizeof(name_msg), "%s\n", argv[1]);
    sendout(fd, name_msg);

    // the main chat loop for sending and receiving messages from the server
    char *line = NULL;
    size_t len = 0;
    // while true
    while (1) {
        // receive and print incoming messages
        recvandprint(fd);
        // read the user's input
        if (getline(&line, &len, stdin) > 1) {
            // send user's message to the server
            sendout(fd, line);
            // exit chatroom on quit
            if (strcmp(line, "quit\n") == 0) break;
        }
    }

    // de-allocate memory, close socket, and finish
    free(line);
    close(fd);
    return 0;
}
