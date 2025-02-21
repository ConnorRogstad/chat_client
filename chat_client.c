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
#include <sys/select.h>
#include <termios.h>

// Server Specifics and Constants
#define SERVER "10.115.12.240"
#define PORT 49153
#define BUFSIZE 1024

// receives data from the server from a socket and stores it up to the buffer size
// this will make sure you receive the messages from other users
// also is the main logic for moving the current message to the next line so that
// the user can continue to edit it while other messages stream in
void recvandprint(int fd, char *input_buffer) {
    static char recv_buffer[BUFSIZE * 2] = ""; // Persistent across function calls
    char buff[BUFSIZE + 1];
    int ret;
    ret = recv(fd, buff, BUFSIZE, 0);
    if (ret > 0) {
        buff[ret] = '\0';
        // Clear current line, print message, and restore input with prompt
        strcat(recv_buffer, buff);
        char *newline;
        // process the message and print full message
        while ((newline = strchr(recv_buffer, '\n')) != NULL) {
            *newline = '\0';
            printf("\r\033[K%s\n", recv_buffer);
            memmove(recv_buffer, newline + 1, strlen(newline + 1) + 1);
        }
        // Restore input prompt
        printf("\r> %s", input_buffer);
        fflush(stdout);
    } else if (ret == -1 && errno != EAGAIN) {
        perror("recv error");
        exit(errno);
    } else if (ret == 0) {
        printf("Server disconnected\n");
        exit(0);
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

    // Set terminal to non-canonical mode
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 1;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    // the main chat loop for sending and receiving messages from the server
    char input_buffer[BUFSIZE] = "";
    int pos = 0;
    fd_set read_fds;

    // while true
    while (1) {
        // check on the input from the user and from the server
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds);
        FD_SET(fd, &read_fds);

        // Wait for activity, and handle errors if they arise
        if (select(fd + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("select error");
            break;
        }

        // receive and print incoming messages
        if (FD_ISSET(fd, &read_fds)) {
            recvandprint(fd, input_buffer);
        }

        // check to see if there is user input to grab
        if (FD_ISSET(STDIN_FILENO, &read_fds)) {
            // read the user's input (single char at a time)
            char c;
            // if there is a char
            if (read(STDIN_FILENO, &c, 1) > 0) {
                // if its the enter key
                if (c == '\n') {
                    // complete message and format it
                    input_buffer[pos] = '\n';
                    input_buffer[pos + 1] = '\0';
                    // send user's message to the server
                    sendout(fd, input_buffer);
                    // exit chatroom on quit
                    if (strcmp(input_buffer, "quit\n") == 0) {
                        char exit_msg[BUFSIZE];
                        snprintf(exit_msg, sizeof(exit_msg), " has left the room.\n");
                        sendout(fd, exit_msg);
                        break;
                    }
                    // move down for user to enter a new message
                    input_buffer[0] = '\0';
                    pos = 0;
                    printf("\r\033[K> ");
                    fflush(stdout);
                }
                // handles the backspace key, so that the user can edit their message
                else if (c == 127) {
                    // Don't delete if there is nothing to delete
                    if (pos > 0) {
                    // move position back and delete char
                        pos--;
                        input_buffer[pos] = '\0';
                        printf("\r\033[K> %s", input_buffer);
                        fflush(stdout);
                    }
                }
                // for the addition of regular chars
                else if (pos < BUFSIZE - 2) {
                    // add it to buffer, and increase position
                    input_buffer[pos] = c;
                    pos++;
                    input_buffer[pos] = '\0';
                    printf("\r\033[K> %s", input_buffer);
                    fflush(stdout);
                }
            }
        }
    }

    // change the terminal setting back to canonical, close socket, and finish
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    close(fd);
    return 0;
}
