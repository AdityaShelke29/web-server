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
#include <poll.h>

#define PORT "6001"
#define BACKLOG 10

// FUNCTION 1: setup_listener(void)
int setup_listener(void) {
    struct addrinfo hints;              // Specify necessary parameters for getaddrinfo()
    struct addrinfo *result;            // Linked list of network address information 
                                        // structs returned by getaddrinfo()
    int return_value;

    memset(&hints, 0, sizeof(hints)); 
    hints.ai_family = AF_UNSPEC; 
    hints.ai_socktype = SOCK_STREAM; 
    hints.ai_flags = AI_PASSIVE;        // Assigns the address of localhost to socket structures

    return_value = getaddrinfo(NULL, PORT, &hints, &result); 

    // Error handling for getaddrinfo()
    if (return_value != 0) {
        printf("Error in call to getaddrinfo(). Exiting...\n"); 
        exit(1);
    }

    // Non - Error Case
    struct addrinfo *curr; 
    int listener_fd; 
    int yes = 1; 

    // Looping through all the network address information structs returned by getaddrinfo(). 
    for (curr = result; curr != NULL; curr = curr->ai_next) {

        // 3 Steps: 
        // 1. Call socket() to create a new socket. 
        // 2. Call setsockopt() to prevent server restart issues. 
        // 3. Call bind() to bind the socket to a specific port. 

        listener_fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol); 

        if (listener_fd < 0) {
            continue;           // Try out next network address information struct with socket(). 
        }

        setsockopt(listener_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

        int bind_return = bind(listener_fd, curr->ai_addr, curr->ai_addrlen); 

        if (bind_return < 0) {
            close(listener_fd); 
            continue;           // Try out next network addreess information struct with bind(). 
        }

        break; 
    }

    freeaddrinfo(result); 

    // Checking if the socket was created and binded correctly. curr == NULL if it was not. 
    if (curr == NULL) {
        printf("No usable network address information struct was found which worked with");
        printf("the calls to socket() and bind().\n"); 
        exit(1); 
    }

    // Calling listen() on the allocated listener_fd file descriptor. Now waiting for 
    // incoming connections that can be accepted. 
    int listener_return = listen(listener_fd, 10); 
    if (listener_return == -1) {
        printf("Error with listen() call.\n"); 
        exit(1); 
    }

    return listener_fd; 
}

// FUNCTION 2: add_fd_to_poll_fds()
void add_fd_to_poll_fds(struct pollfd *poll_fds[], int new_fd, int *count, int *size) {
    if (*count == *size) {
        *size = *size * 2; 
        *poll_fds = realloc(*poll_fds, sizeof(struct pollfd*) * (*size)); 
    }

    (*poll_fds)[*count].fd = new_fd; 
    (*poll_fds)[*count].events = POLLIN; 

    *count = *count + 1; 
}

// FUNCTION 3: delete_fd_from_poll_fds()
void delete_fd_from_poll_fds(struct pollfd poll_fds[], int curr, int *count) {

    // Take the last file descriptor and replace the one to be deleted with it. 
    // Now, we reduce the count by 1, so that there are not two references to 
    // the last file descriptor. 
    poll_fds[curr] = poll_fds[*count - 1]; 
    *count = *count - 1; 
}

// MAIN FUNCTION: 
int main(void) {
    int listener; 

    // Create listener socket and assign it to a file descriptor. 
    listener = setup_listener(); 

    int size = 2; 
    int count = 0; 
    struct pollfd *poll_fds = malloc(sizeof(struct pollfd) * size); 

    // Our listener is our first file descriptor which is attached to a socket. 
    // Thus, we want to handle new connections that this listener picks up. 
    // Thus, we add the listener as the first connection in our array of fds. 
    poll_fds[0].fd = listener; 
    poll_fds[0].events = POLLIN; 
    count = count + 1; 

    // BUFFER for storing incoming messages
    char buffer[5000]; 

    // Main Repeating Loop
    while(1) {
        
        // Execution blocks here until poll returns, which only happens 
        // after the timeout or after an incoming signal is found. 
        int poll_return = poll(poll_fds, count, -1); 

        if (poll_return == -1) {
            printf("Error with call to poll() function.\n"); 
            exit(1); 
        }

        // Iterate through all of the file descriptors. 
        for (int i = 0; i < count; i++) {
            
            // If there is a file descriptor has the POLLIN flag in its 
            // revents field, then its corresponding socket has received data. 
            if (poll_fds[i].revents & POLLIN) {

                // Case 1: The file descriptor which has received data is the 
                // one associated with the listener. A new connection must be 
                // handled. 
                if (poll_fds[i].fd == listener) {
                    struct sockaddr_storage new_connection_address; 
                    socklen_t new_address_length = sizeof(new_connection_address);

                    int new_fd = accept(listener, (struct sockaddr *)
                        &new_connection_address, &new_address_length); 
                    
                    // Error case after accept()
                    if (new_fd == -1) {
                        printf("New client detected, but error occured in accept() function.\n"); 
                    }

                    // Non - Error case after accept()
                    else {
                        // Add new file descriptor to the list poll_fds so that we can 
                        // continuously monitor the new file descriptor for incoming data. 
                        add_fd_to_poll_fds(&poll_fds, new_fd, &count, &size); 
                        printf("Added new connection. This is client number %d\n", count); 
                    }
                }

                // Case 2: The file descriptor which has received data is not 
                // the listener. We are expecting a text message here. 
                // Get the text message and send it to the rest of the 
                // connected clients. 
                else {
                    int received_message_length = recv(poll_fds[i].fd, buffer, sizeof(buffer), 0); 
                    int client_fd = poll_fds[i].fd; 

                    // Error and closed connection cases. 
                    if (received_message_length <= 0) {
                        if (received_message_length == 0) {
                            printf("Client at file directory %d has been disconnected\n", client_fd); 
                        }
                        else {
                            printf("An error has occured with the recv() function.\n"); 
                        }

                        // Now that the connection has either closed or there is an error 
                        // preventing communication, the socket is closed. 
                        close(poll_fds[i].fd); 

                        // Also, remove the file descriptor for the list of file descriptors that 
                        // the poll function should monitor. 
                        delete_fd_from_poll_fds(poll_fds, i, &count); 
                    }
                    
                    else {
                        for (int j = 0; j < count; j++) {
                            if (poll_fds[j].fd != listener && poll_fds[j].fd != client_fd) {
                                
                                int send_return_value = send(poll_fds[j].fd, 
                                    buffer, received_message_length, 0); 

                                if (send_return_value == -1) {
                                    printf("Error with send().\n"); 
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}