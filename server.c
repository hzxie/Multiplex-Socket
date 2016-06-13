#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

#define MAX_PENDING_CONNECTIONS 32
#define MAX_MESSAGE_SIZE        128

/**
 * The entrance of the server application.
 * 
 * @param  argc the number of arguments
 * @param  argv a pointer to a char array that stores arguments
 * @return 0 if the application exited normally
 */
int main(int argc, char* argv[]) {
    if ( argc != 2 ) {
        fprintf(stderr, "Usage: %s PortNumber\n", argv[0]);
        exit(1);
    } 

    int portNumber = atoi(argv[1]);
    if ( portNumber <= 0 ) {
        fprintf(stderr, "Usage: %s PortNumber\n", argv[0]);
        exit(1);
    }

    /*
     * Create socket file descriptor.
     * Function Prototype: int socket(int domain, int type,int protocol)
     * Defined in sys/socket.h
     *
     * @param domain:   AF_INET stands for Internet, AF_UNIX can only communicate between UNIX systems.
     * @param type      the prototype to use, SOCK_STREAM stands for TCP and SOCK_DGRAM stands for UDP
     * @param protocol  if type is specified, this parameter can be assigned to 0.
     * @return -1 if socket is failed to create
     */
    int tcpSocketFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    int udpSocketFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0);

    if ( tcpSocketFileDescriptor == -1 || udpSocketFileDescriptor == -1 ) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        exit(127);
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
    int sockaddrSize = sizeof(struct sockaddr);
    struct sockaddr_in serverSocketAddress;
    bzero(&serverSocketAddress, sockaddrSize);
    serverSocketAddress.sin_family = AF_INET;
    serverSocketAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverSocketAddress.sin_port = htons(portNumber);

    /*
     * Set reuse of address and port options for socket.
     * Function Prototype: int setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen);
     * Defined in sys/socket.h and sys/types.h
     *
     * @param sockfd  the socket file descriptor
     * @param level   the level at which the option resides, SOL_SOCKET means API level, IPPROTO_IP and IPPROTO_TCP 
     *                stand for IP and TCP level respectively
     * @param optname SO_REUSERADDR controls whether bind should permit reuse of local addresses for this socket.
     *                See http://www.gnu.org/software/libc/manual/html_node/Socket_002dLevel-Options.html for details.
     * @param optval  the value of the option
     * @param optlen  the size of the option
     * @return -1 if the operation failed
     */
    int optionValue = 1;
    setsockopt(tcpSocketFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &optionValue, sizeof(optionValue));

    
    /*
     * Bind socket file descriptor to the port.
     * Function Prototype: int bind(int sockfd, struct sockaddr *my_addr, int addrlen)
     * Defined in sys/socket.h and sys/types.h
     *
     * @param sockfd  the socket file descriptor
     * @param my_addr the specified address
     * @param addrlen the size of the struct sockaddr
     * @return -1 if socket is failed to bind
     */
    if ( bind(tcpSocketFileDescriptor, (struct sockaddr*)(&serverSocketAddress), sockaddrSize) == -1) {
        fprintf(stderr, "Failed to bind TCP socket file descriptor to specified address: %s\n", strerror(errno));
        exit(127);
    }
    if ( bind(udpSocketFileDescriptor, (struct sockaddr*)(&serverSocketAddress), sockaddrSize) == -1) {
        fprintf(stderr, "Failed to bind UDP socket file descriptor to specified address: %s\n", strerror(errno));
        exit(127);
    }

    /*
     * Listen for connections on a socket.
     * Function Prototype: int listen(int sockfd,int backlog)
     * Defined in sys/socket.h and sys/types.h
     *
     * @param sockfd  the socket file descriptor
     * @param backlog the maximum length to which the queue of pending connections
     * @return -1 if socket is failed to listen
     */
    if ( listen(tcpSocketFileDescriptor, MAX_PENDING_CONNECTIONS) == -1 ) {
        fprintf(stderr, "Failed to listen to the TCP socket: %s\n", strerror(errno));
        exit(127);
    }

    /*
     * Infinite Loop for receiving connections from clients. 
     */
    while ( 1 ) {
        struct sockaddr_in clientSocketAddress;
        int clientSocketFileDescriptor = accept(tcpSocketFileDescriptor, (struct sockaddr *)(&clientSocketAddress), &sockaddrSize);

        if ( clientSocketFileDescriptor == -1 ) {
            fprintf(stderr, "Failed to accpet a socket from client: %s\n", strerror(errno));
            continue;
        }
        
        // Display logs in console
        fprintf(stderr, "Connection established with %s\n", inet_ntoa(clientSocketAddress.sin_addr));

        // Send message to client
        char message[MAX_MESSAGE_SIZE] = "Hello";
        if ( write(clientSocketFileDescriptor, message, strlen(message)) == -1 ) {
            fprintf(stderr, "Failed to send a message to client: %s\n", strerror(errno));
        }

        // Close socket for this client
        close(clientSocketFileDescriptor);
    }

    /*
     * Close Sockets.
     */
    close(tcpSocketFileDescriptor);
    close(udpSocketFileDescriptor);

    return 0;
}