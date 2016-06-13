#include <errno.h>
#include <fcntl.h>      // for opening socket
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>     // for closing socket
#include <sys/socket.h>
#include <sys/types.h>

#define BUFFER_SIZE 1024

/**
 * The entrance of the server application.
 * 
 * @param  argc the number of arguments
 * @param  argv a pointer to a char array that stores arguments
 * @return 0 if the application exited normally
 */
int main(int argc, char *argv[]) {
    if ( argc != 3 ) {
        fprintf(stderr," Usage: %s Host PortNumber\n",argv[0]);
        return EXIT_FAILURE;
    }
    
    struct hostent* pHost = gethostbyname(argv[1]);
    if ( pHost == NULL ) {
        fprintf(stderr, "Usage: %s Host PortNumber\n", argv[0]);
        return EXIT_FAILURE;
    }
    int portNumber = atoi(argv[2]);
    if ( portNumber <= 0 ) {
        fprintf(stderr, "Usage: %s Host PortNumber\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*
     * Create socket file descriptor.
     * Function Prototype: int socket(int domain, int type,int protocol)
     * Defined in sys/socket.h
     *
     * @param domain:   AF_INET stands for Internet, AF_UNIX can only communicate between UNIX systems.
     * @param type      the prototype to use, SOCK_STREAM stands for udp and SOCK_DGRAM stands for UDP
     * @param protocol  if type is specified, this parameter can be assigned to 0.
     * @return -1 if socket is failed to create
     */
    int udpSocketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0);
    if ( udpSocketFileDescriptor == -1 ) {
        fprintf(stderr, "[ERROR] Failed to create socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /*
     * Initialize sockaddr struct.
     *
     * The structure of sockaddr:
     * 
     * struct sockaddr{
     *     unisgned short  as_family;
     *     char            sa_data[14];
     * };
     *
     * To keep compatibility in different OS, sockaddr_in is used:
     *
     * struct sockaddr_in{
     *     unsigned short          sin_family;     // assigned to AF_INET generally
     *     unsigned short int      sin_port;       // the port number listens to
     *     struct in_addr          sin_addr;       // assigned to INADDR_ANY for communicating with any hosts
     *     unsigned char           sin_zero[8];    // stuffing bits
     * };
     *
     * Both of them are defined in netinet/in.h
     */
    socklen_t sockaddrSize = sizeof(struct sockaddr);
    struct sockaddr_in serverSocketAddress;
    bzero(&serverSocketAddress, sockaddrSize);
    serverSocketAddress.sin_family = AF_INET;
    serverSocketAddress.sin_addr=*((struct in_addr *)pHost->h_addr);
    serverSocketAddress.sin_port = htons(portNumber);

    char inputBuffer[BUFFER_SIZE] = {0};
    char outputBuffer[BUFFER_SIZE] = {0};
    fprintf(stderr, "[INFO] Congratulations! Connection established with server.\nType \'BYE\' to disconnect.\n");
    do {
        // Send a message to client
        fprintf(stderr, "> ");
        scanf("%s", outputBuffer);

        if ( sendto(udpSocketFileDescriptor, outputBuffer, strlen(outputBuffer) + 1, 
                0, (struct sockaddr *)(&serverSocketAddress), sockaddrSize) == -1 ) {
            fprintf(stderr, "[ERROR] An error occurred while sending message to the server: %s\nThe connection is going to close.\n", strerror(errno));
            break;
        }

        // Receive a message from client
        int readBytes = recvfrom(udpSocketFileDescriptor, inputBuffer, BUFFER_SIZE, 
                            0, (struct sockaddr *)(&serverSocketAddress), &sockaddrSize);
        if ( readBytes < 0 ) {
            fprintf(stderr, "[ERROR] An error occurred while receiving message from the server: %s\nThe connection is going to close.\n", strerror(errno));
            break;
        }
        fprintf(stderr, "[INFO] Received a message from server: %s\n", inputBuffer);

        // Stop sending message to server
        if ( strcmp("BYE", outputBuffer) == 0 ) {
            break;
        }
    } while ( 1 );

    /*
     * Close socket for client.
     */
    close(udpSocketFileDescriptor);


    return EXIT_SUCCESS;
}