#include <errno.h>
#include <fcntl.h>      // for opening socket
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>     // for closing socket
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>


#define TRUE                    1
#define FALSE                   0

#define MAX_PENDING_CONNECTIONS 4
#define MAX_CONNECTIONS         32
#define BUFFER_SIZE             1025

/**
 * Fix undefined reference to `max' issue.
 */
static inline int max(int a, int b) {
    return (a > b) ? a : b;
}

/**
 * Prototypes of functions.
 */
int acceptConnections(int tcpSocketFileDescriptor, int udpSocketFileDescriptor);
int registerNewSocket(int clientSocketFD, int* clientSocketFileDescriptors, int n);

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
        return EXIT_FAILURE;
    } 

    int portNumber = atoi(argv[1]);
    if ( portNumber <= 0 ) {
        fprintf(stderr, "Usage: %s PortNumber\n", argv[0]);
        return EXIT_FAILURE;
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
        fprintf(stderr, "[ERROR] Failed to bind TCP socket file descriptor to specified address: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    if ( bind(udpSocketFileDescriptor, (struct sockaddr*)(&serverSocketAddress), sockaddrSize) == -1) {
        fprintf(stderr, "[ERROR] Failed to bind UDP socket file descriptor to specified address: %s\n", strerror(errno));
        return EXIT_FAILURE;
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
        fprintf(stderr, "[ERROR] Failed to listen to the TCP socket: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }

    /*
     * Prepare for handling TCP and UDP connections using select.
     */
    int exitCode = acceptConnections(tcpSocketFileDescriptor, udpSocketFileDescriptor);
    if ( exitCode == -1 ) {
        fprintf(stderr, "[ERROR] Server exit with an error: %s\n", strerror(errno));
    }

    /*
     * Close Sockets.
     *
     * close(tcpSocketFileDescriptor);
     * close(udpSocketFileDescriptor);
     */
    return EXIT_SUCCESS;
}

/**
 * Connections handler for the server.
 * @param  tcpSocketFileDescriptor the file descriptor of TCP socket
 * @param  udpSocketFileDescriptor the file descriptor of UDP socket
 * @return -1 if a severe error occurred in this procedure
 */
int acceptConnections(int tcpSocketFileDescriptor, int udpSocketFileDescriptor) {
    /**
     * The file descriptor used to check readability, if characters become available for reading.
     */
    fd_set readFileDescriptorSet;

    /**
     * Buffers for sending and receiving data.
     */
    char inputBuffer[BUFFER_SIZE] = {0};
    char outputBuffer[BUFFER_SIZE] = {0};

    /**
     * Client sockets.
     */
    socklen_t sockaddrSize = sizeof(struct sockaddr);
    struct sockaddr_in clientSocketAddress;
    int clientSocketFileDescriptors[MAX_CONNECTIONS] = {0};

    /**
     * Handle TCP and UDP connections.
     */
    while ( TRUE ) {
        /**
         * Clear the read file descriptor set.
         */
        FD_ZERO(&readFileDescriptorSet);

        /**
         * Add file descriptor of TCP and UDP to the set.
         */
        FD_SET(tcpSocketFileDescriptor, &readFileDescriptorSet);
        FD_SET(udpSocketFileDescriptor, &readFileDescriptorSet);
        int maxFileDescriptor = max(tcpSocketFileDescriptor, udpSocketFileDescriptor);

        /**
         * Add valid socket file descriptor to the read set.
         */
        int i = 0;
        for ( i = 0; i < MAX_CONNECTIONS; ++ i ) {
            int clientSocketFD = clientSocketFileDescriptors[i];

            if ( clientSocketFD > 0 ) {
                FD_SET(clientSocketFD, &readFileDescriptorSet);
            }
            if ( clientSocketFD > maxFileDescriptor ) {
                maxFileDescriptor = clientSocketFD;
            }
        }

        /*
         * Wait for an activity on one of the sockets, timeout is NULL, so wait indefinitely.
         * Function Prototype: int select(int nfds, fd_set *readset, fd_set *writeset,fd_set* exceptset, struct tim *timeout);
         * Defined in sys/select.h
         *
         * @param nfds      the max ID of file descriptor to check
         * @param readset   the file descriptor used to check readability
         * @param writeset  the file descriptor used to check writability
         * @param exceptset the file descriptor used to check exceptions
         * @param timeout   the interval to be rounded up
         * @return the number of file descriptors contained in the three returned descriptor sets
         */
        int events = select(maxFileDescriptor + 1, &readFileDescriptorSet, NULL, NULL, NULL);
        if ( events == -1 && errno != EINTR ) {
            fprintf(stderr, "[ERROR] An error occurred while monitoring sockets: %s\n", strerror(errno));
        }

        // New incoming TCP connection
        if ( FD_ISSET(tcpSocketFileDescriptor, &readFileDescriptorSet) ) {
            // Establish connection with client
            int clientSocketFD = accept(tcpSocketFileDescriptor, (struct sockaddr *)(&clientSocketAddress), &sockaddrSize);

            if ( clientSocketFD == -1 ) {
                fprintf(stderr, "[WARN] Failed to accpet a socket from client: %s\n", strerror(errno));
                continue;
            }
            fprintf(stderr, "[INFO] Connection established with %s:%d\n", 
                inet_ntoa(clientSocketAddress.sin_addr), ntohs(clientSocketAddress.sin_port));

            // Send welcome message to client
            if ( send(clientSocketFD, "ACCEPT", strlen("ACCEPT") + 1, 0) == -1 ) {
                fprintf(stderr, "[ERROR] An error occurred while sending message to the client: %s\nThe connection is going to close.\n", strerror(errno));
                continue;
            }

            // Register file descriptors for sockets
            int clientSocketFDIndex = registerNewSocket(clientSocketFD, clientSocketFileDescriptors, MAX_CONNECTIONS);
            if ( clientSocketFDIndex == -1 ) {
                fprintf(stderr, "[WARN] Failed to register the socket for client: %s:%d\n", 
                    inet_ntoa(clientSocketAddress.sin_addr), ntohs(clientSocketAddress.sin_port));
            } else {
                fprintf(stderr, "[INFO] Socket #%d registered for the client: %s:%d\n", 
                    clientSocketFDIndex, inet_ntoa(clientSocketAddress.sin_addr), ntohs(clientSocketAddress.sin_port));
            }
        }
        
        // New incoming UDP connection
        if ( FD_ISSET(udpSocketFileDescriptor, &readFileDescriptorSet) ) {
            // TODO: Handle UDP connections
        }

        // IO operation on some other sockets
        for ( i = 0; i < MAX_CONNECTIONS; ++ i ) {
            int clientSocketFD = clientSocketFileDescriptors[i];

            if ( FD_ISSET(clientSocketFD, &readFileDescriptorSet) ) {
                int readBytes = recv(clientSocketFD, inputBuffer, BUFFER_SIZE, 0);
                
                if ( readBytes < 0 ) {
                    fprintf(stderr, "[ERROR] An error occurred while sending message to the client [%s:%d]: %s\nThe connection is going to close.\n", 
                        inet_ntoa(clientSocketAddress.sin_addr), ntohs(clientSocketAddress.sin_port), strerror(errno));
                    continue;
                }
                fprintf(stderr, "[INFO] Received a message from client [%s:%d]: %s\n", 
                    inet_ntoa(clientSocketAddress.sin_addr), ntohs(clientSocketAddress.sin_port), inputBuffer);

                // Complete receiving message from client
                if ( readBytes == 0 || strcmp("BYE", inputBuffer) == 0 ) {
                    close(clientSocketFD);
                    clientSocketFileDescriptors[i] = 0;

                    continue;
                }

                // Send a message to client
                strcpy(outputBuffer, inputBuffer);
                if ( send(clientSocketFD, outputBuffer, strlen(outputBuffer) + 1, 0) == -1 ) {
                    clientSocketFileDescriptors[i] = 0;

                    fprintf(stderr, "[ERROR] An error occurred while sending message to the client [%s:%d]: %s\nThe connection is going to close.\n", 
                        inet_ntoa(clientSocketAddress.sin_addr), ntohs(clientSocketAddress.sin_port), strerror(errno));
                }
            } else {

            }
        }
    }
}

/**
 * Register a new file descriptor for new connections.
 * @param  clientSocketFD              the file descriptor to register
 * @param  clientSocketFileDescriptors the set of file descriptors
 * @param  n                           the capcity of the set of file descriptors
 * @return the index of the file descriptor in the set
 */
int registerNewSocket(int clientSocketFD, int* clientSocketFileDescriptors, int n) {
    int i = 0;
    for ( i = 0; i < n; ++ i ) {
        if ( clientSocketFileDescriptors[i] == 0 ) {
            clientSocketFileDescriptors[i] = clientSocketFD;
            
            return i;
        }
    }
    return -1;
}