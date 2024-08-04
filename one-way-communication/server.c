// This is the server of a one way communication pipeline. 
// It implements the socket API which allows for messages 
// to be sent between two hosts. Please run the compiled 
// executable for endpoint 1 and endpoint 2 to send messages 
// between these hosts. 

// Overall Idea: 
// Server waits for endpoint 1 to connect. 
// Server waits for endpoint 2 to connect. 
// When both endpoints are connected, whenever endpoint 1 
// sends a message, endpoint 2 should receive that message. 

// How to Test: 
// Run "netcat localhost 6000" in one terminal. 
// Run "netcat localhost 6000" in another terminal. 
// Use the first terminal to send a message and ensure that 
// the message arrives in the other terminal, as part of the 
// one way communication network that server.c has set up. 

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define PORT "6000"
#define BACKLOG 10

int main(void) {
    struct addrinfo hints, *res, *response; 
    int return_value; 
    int sockfd; 

    memset(&hints, 0, sizeof hints); 
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    // Creates a struct addrinfo which represents a network location 
    // of the current machine's IP address, port 6000, with a 
    // stream socket. 

    if ((return_value = getaddrinfo(NULL, PORT, &hints, &res)) != 0) {
        printf("Step 1 failed: getaddrinfo() did not work correctly."); 
        return 1; 
    }

    // Now, res contains a linked list of many possible structs representing 
    // network locations with the given parameters. We iterate through these 
    // to find one where we can call socket() and bind() the socket to that 
    // network location. 

    for (response = res; response != NULL; response = response->ai_next) {
        
        // Creates a socket using information from the newly created struct. 
        // The integer sockfd now contains the file descriptor to that socket. 
        sockfd = socket(response->ai_family, response->ai_socktype, response->ai_protocol); 

        if (sockfd == -1) {
            perror("There was an error with the socket() function."); 
            continue; 
        }

        // [TBD] [ADD CODE FOR setsockopt() HERE]
        int yes = 1; 
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }

        // Binds the newly created socket file descriptor to the internet address 
        // that consists of the current machine's IP address + the given port to 
        // getaddrinfo(). We find this in res
        int bind_return = bind(sockfd, response->ai_addr, response->ai_addrlen); 

        if (bind_return == -1) {
            close(sockfd); 
            perror("There was an error with the bind() function."); 
            continue; 
        }

        break; 

    }

    // Frees up the structure set by getaddrinfo(). 
    freeaddrinfo(res); 

    // Main listener loop -- need to wait for two connections
    struct sockaddr_storage incoming_connection_address1; 
    struct sockaddr_storage incoming_connection_address2; 

    int fd1 = -1; 
    int fd2 = -1; 

    printf("Calling listen() function to wait for incoming connections.\n"); 
    fflush(stdin); 

    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    printf("Call to listen() finished successfully.\n\n"); 
    fflush(stdin); 

    while(1) {

        if (fd1 == -1) {
            socklen_t incoming_address_length = sizeof incoming_connection_address1; 
            fd1 = accept(sockfd, (struct sockaddr *) &incoming_connection_address1, &incoming_address_length); 
            printf("Connection achieved with fd1 (messenger): %d\n", fd1); 
        }

        if (fd2 == -1) {
            socklen_t incoming_address_length = sizeof incoming_connection_address2;
            fd2 = accept(sockfd, (struct sockaddr *) &incoming_connection_address2, &incoming_address_length); 
            printf("Connection achieved with fd2 (receiver): %d\n", fd2); 
        }

        if (fd1 != -1 && fd2 != -1) {
            printf("Both connections open -- fd1: %d, fd2: %d\n", fd1, fd2); 

            char msg[1000]; 
            int len1, len2; 

            len1 = recv(fd1, (void *)msg, sizeof(msg), 0); 

            if (len1 != -1) {

                if (len1 != -1) {
                    send(fd2, msg, len1, 0); 
                }
            }
        }
    }

    return 0; 

}