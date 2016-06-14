#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/if_ether.h>
#include <netinet/ip_icmp.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define FALSE           0
#define TRUE            1
#define BUFFER_SIZE     65536
#define CHARS_PER_LINE  16

/**
 * Prototypes of functions.
 */
unsigned short getIpHeaderLength(unsigned char* buffer);
void parseDataPacket(unsigned char* buffer, int packetSize, FILE* logFile);
void parseEthernetHeader(unsigned char* buffer, int packetSize, FILE* logFile);
void parseIpHeader(unsigned char* buffer, int packetSize, FILE* logFile);
void parseIcmpPacket(unsigned char* buffer, int packetSize, FILE* logFile);
void parseTcpPacket(unsigned char* buffer, int packetSize, FILE* logFile);
void parseUdpPacket(unsigned char* buffer, int packetSize, FILE* logFile);
void parseDataPayload(unsigned char* buffer, int packetSize, FILE* logFile);

/**
 * The entrance of the server application.
 * Reference:
 * - http://www.binarytides.com/packet-sniffer-code-c-linux/
 * 
 * @param  argc the number of arguments
 * @param  argv a pointer to a char array that stores arguments
 * @return 0 if the application exited normally
 */
int main(int argc, char* argv[]) {
    // Buffer for receiving packets
    unsigned char* buffer = (unsigned char*) malloc(BUFFER_SIZE);
    
    // File descriptor for the log file
    FILE* logFile = fopen("packet-sniffer.log", "w");

    if ( logFile == NULL ) {
        fprintf(stderr, "[ERROR] Unable to create packet-sniffer.log file.\n");
        return EXIT_FAILURE;
    }

    // Create file descriptor for raw socket
    int rawSocketFileDescriptor = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL)) ;
    if ( rawSocketFileDescriptor == -1 ) {
        fprintf(stderr, "[ERROR] Unable to create row socket: %s.\n", strerror(errno));
        return EXIT_FAILURE;
    }

    // Start sniffing packets
    fprintf(stderr, "[INFO] Start sniffing packets, packets will be saved to packet-sniffer.log file.\n");
    while ( TRUE ) {
        struct sockaddr_in serverSocketAddr;
        socklen_t sockaddrSize = sizeof(struct sockaddr);

        // Receive a packet
        int receivedDataSize = recvfrom(rawSocketFileDescriptor, buffer, BUFFER_SIZE, 0, 
                                (struct sockaddr *)(&serverSocketAddr), &sockaddrSize);
        if ( receivedDataSize < 0 ) {
            fprintf(stderr, "[ERROR] An error occurred while receiving data packets: %s.\n", strerror(errno));
            return EXIT_FAILURE;
        }

        // Parse data packet
        parseDataPacket(buffer, receivedDataSize, logFile);
    }

    // Close file descriptor for file and socket
    close(rawSocketFileDescriptor);
    fclose(logFile);

    return EXIT_SUCCESS;
}

/**
 * Detect the type of the packet, and parse them respectively.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseDataPacket(unsigned char* buffer, int packetSize, FILE* logFile) {
    /*
     * Static variables for statistics.
     */
    static int tcpPacketsReceived = 0,
               udpPacketsReceived = 0,
               icmpPacketsReceived = 0,
               igmpPacketsReceived = 0,
               otherPacketsReceived = 0,
               totalPacketsReceived = 0;

    ++ totalPacketsReceived;

    /*
     * Parse IP protocol of this packet, where ethernet header is excluded.
     */
    struct iphdr *iph = (struct iphdr*)(buffer + sizeof(struct ethhdr));

    switch ( iph->protocol ) {
        case IPPROTO_ICMP:
            ++ icmpPacketsReceived;
            parseIcmpPacket(buffer, packetSize, logFile);

            break;

        case IPPROTO_IGMP:
            ++ igmpPacketsReceived;
            break;
        
        case IPPROTO_TCP:
            ++ tcpPacketsReceived;
            parseTcpPacket(buffer, packetSize, logFile);

            break;
        
        case IPPROTO_UDP:
            ++ udpPacketsReceived;
            parseUdpPacket(buffer, packetSize, logFile);

            break;
        
        default:
            ++ otherPacketsReceived;
            break;
    }
    printf("Packets Stat: TCP : %d   UDP : %d   ICMP : %d   IGMP : %d   Others : %d   Total : %d\r", 
        tcpPacketsReceived, udpPacketsReceived, icmpPacketsReceived, igmpPacketsReceived, 
        otherPacketsReceived, totalPacketsReceived);
}

/**
 * Parse the header of the ethernet packet.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseEthernetHeader(unsigned char* buffer, int packetSize, FILE* logFile) {
    struct ethhdr* eth = (struct ethhdr *) buffer;

    fprintf(logFile, "\n");
    fprintf(logFile, "Ethernet Header\n");
    fprintf(logFile, "   |-Destination Address  : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth->h_dest[0], eth->h_dest[1], eth->h_dest[2], eth->h_dest[3], eth->h_dest[4], eth->h_dest[5]);
    fprintf(logFile, "   |-Source Address       : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X \n", eth->h_source[0], eth->h_source[1], eth->h_source[2], eth->h_source[3], eth->h_source[4], eth->h_source[5]);
    fprintf(logFile, "   |-Protocol             : %u \n", (unsigned short)eth->h_proto);
}

/**
 * Parse the header of the IP packet.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseIpHeader(unsigned char* buffer, int packetSize, FILE* logFile) {
    /**
     * Parse Ethernet Header
     */
    parseEthernetHeader(buffer, packetSize, logFile);

    struct iphdr* iph = (struct iphdr*)(buffer + sizeof(struct ethhdr));

    // Parse the source and destination of the IP packet
    socklen_t sockaddrSize = sizeof(struct sockaddr);
    struct sockaddr_in srcSocketAddr, destSocketAddr;

    memset(&srcSocketAddr, 0, sockaddrSize);
    memset(&destSocketAddr, 0, sockaddrSize);

    srcSocketAddr.sin_addr.s_addr = iph->saddr;
    destSocketAddr.sin_addr.s_addr = iph->daddr;

    // Parse other information in IP packet
    fprintf(logFile, "\n");
    fprintf(logFile, "IP Header\n");
    fprintf(logFile, "   |-IP Version           : %d\n", (unsigned int) iph->version);
    fprintf(logFile, "   |-IP Header Length     : %d Bytes\n", ((unsigned int) (iph->ihl)) * 4);
    fprintf(logFile, "   |-Type Of Service      : %d\n", (unsigned int)iph->tos);
    fprintf(logFile, "   |-IP Total Length      : %d Bytes\n", ntohs(iph->tot_len));
    fprintf(logFile, "   |-Identification       : %d\n", ntohs(iph->id));
    fprintf(logFile, "   |-TTL                  : %d\n", (unsigned int) iph->ttl);
    fprintf(logFile, "   |-Protocol             : %d\n", (unsigned int) iph->protocol);
    fprintf(logFile, "   |-Checksum             : %d\n", ntohs(iph->check));
    fprintf(logFile, "   |-Source IP            : %s\n", inet_ntoa(srcSocketAddr.sin_addr));
    fprintf(logFile, "   |-Destination IP       : %s\n", inet_ntoa(destSocketAddr.sin_addr));
}

/**
 * Get the length of IP header.
 * @param buffer     the buffer for receiving data
 * @return the length of IP header
 */
unsigned short getIpHeaderLength(unsigned char* buffer) {
    struct iphdr* iph = (struct iphdr*)(buffer + sizeof(struct ethhdr));
    return iph->ihl * 4;
}

/**
 * Parse the ICMP packet.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseIcmpPacket(unsigned char* buffer, int packetSize, FILE* logFile) {
    unsigned short iphdrlen = getIpHeaderLength(buffer);

    struct icmphdr *icmph = (struct icmphdr*)(buffer + iphdrlen  + sizeof(struct ethhdr));
    unsigned short hdrlen = sizeof(struct ethhdr) + iphdrlen + sizeof(struct icmphdr);

    fprintf(logFile, "\n***********************ICMP Packet************************\n");

    // Parse ethernet packet and IP packet first.
    parseIpHeader(buffer, packetSize, logFile);

    // Parse ICMP packet
    fprintf(logFile, "\n");
    fprintf(logFile, "ICMP Header\n");
    fprintf(logFile, "   |-Type                 : %d ", (unsigned int)(icmph->type));

    if ( (unsigned int)(icmph->type) == 11 ) {
        fprintf(logFile, "  (TTL Expired)\n");
    } else if ( (unsigned int)(icmph->type) == ICMP_ECHOREPLY ) {
        fprintf(logFile, "  (ICMP Echo Reply)\n");
    } else {
        fprintf(logFile, "\n");
    }

    fprintf(logFile, "   |-Code                 : %d\n", (unsigned int)(icmph->code));
    fprintf(logFile, "   |-Checksum             : %d\n", ntohs(icmph->checksum));
    fprintf(logFile, "\n");

    fprintf(logFile, "IP Header\n");
    parseDataPayload(buffer, iphdrlen, logFile);
         
    fprintf(logFile, "TCP Header\n");
    parseDataPayload(buffer + iphdrlen, sizeof(struct icmphdr), logFile);
         
    fprintf(logFile, "Data Payload\n");    
    parseDataPayload(buffer + hdrlen, packetSize - hdrlen, logFile);
}

/**
 * Parse the TCP packet.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseTcpPacket(unsigned char* buffer, int packetSize, FILE* logFile) {
    unsigned short iphdrlen = getIpHeaderLength(buffer);

    struct tcphdr* tcph = (struct tcphdr*)(buffer + iphdrlen + sizeof(struct ethhdr));
    unsigned short hdrlen = sizeof(struct ethhdr) + iphdrlen + tcph->doff * 4;

    fprintf(logFile, "\n***********************TCP Packet*************************\n");

    // Parse ethernet packet and IP packet first.
    parseIpHeader(buffer, packetSize, logFile);

    // Parse TCP packet
    fprintf(logFile, "\n");
    fprintf(logFile, "TCP Header\n");
    fprintf(logFile, "   |-Source Port          : %u\n", ntohs(tcph->source));
    fprintf(logFile, "   |-Destination Port     : %u\n", ntohs(tcph->dest));
    fprintf(logFile, "   |-Sequence Number      : %u\n", ntohl(tcph->seq));
    fprintf(logFile, "   |-Acknowledge Number   : %u\n", ntohl(tcph->ack_seq));
    fprintf(logFile, "   |-Header Length        : %d Bytes\n" , (unsigned int)tcph->doff * 4);
    fprintf(logFile, "   |-Urgent Flag          : %d\n", (unsigned int)tcph->urg);
    fprintf(logFile, "   |-Acknowledgement Flag : %d\n", (unsigned int)tcph->ack);
    fprintf(logFile, "   |-Push Flag            : %d\n", (unsigned int)tcph->psh);
    fprintf(logFile, "   |-Reset Flag           : %d\n", (unsigned int)tcph->rst);
    fprintf(logFile, "   |-Synchronise Flag     : %d\n", (unsigned int)tcph->syn);
    fprintf(logFile, "   |-Finish Flag          : %d\n", (unsigned int)tcph->fin);
    fprintf(logFile, "   |-Window               : %d\n", ntohs(tcph->window));
    fprintf(logFile, "   |-Checksum             : %d\n", ntohs(tcph->check));
    fprintf(logFile, "   |-Urgent Pointer       : %d\n", tcph->urg_ptr);
    fprintf(logFile, "\n                        DATA Dump                         \n");
         
    fprintf(logFile, "IP Header\n");
    parseDataPayload(buffer, iphdrlen, logFile);
         
    fprintf(logFile, "TCP Header\n");
    parseDataPayload(buffer + iphdrlen, tcph->doff * 4, logFile);
         
    fprintf(logFile, "Data Payload\n");    
    parseDataPayload(buffer + hdrlen, packetSize - hdrlen, logFile);
}

/**
 * Parse the UDP packet.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseUdpPacket(unsigned char* buffer, int packetSize, FILE* logFile) {
    unsigned short iphdrlen = getIpHeaderLength(buffer);
    struct udphdr* udph = (struct udphdr*)(buffer + iphdrlen  + sizeof(struct ethhdr));
    unsigned short hdrlen = sizeof(struct ethhdr) + iphdrlen + sizeof(struct udphdr);
    
    fprintf(logFile, "\n***********************UDP Packet*************************\n");

    // Parse ethernet packet and IP packet first.
    parseIpHeader(buffer, packetSize, logFile);

    // Parse UDP packet
    fprintf(logFile, "\nUDP Header\n");
    fprintf(logFile, "   |-Source Port          : %d\n", ntohs(udph->source));
    fprintf(logFile, "   |-Destination Port     : %d\n", ntohs(udph->dest));
    fprintf(logFile, "   |-UDP Length           : %d\n", ntohs(udph->len));
    fprintf(logFile, "   |-UDP Checksum         : %d\n", ntohs(udph->check));
    fprintf(logFile, "\n                        DATA Dump                         \n");
     
    fprintf(logFile, "IP Header\n");
    parseDataPayload(buffer, iphdrlen, logFile);

    fprintf(logFile, "UDP Header\n");
    parseDataPayload(buffer + iphdrlen, sizeof(struct udphdr), logFile);

    fprintf(logFile, "Data Payload\n");    
    parseDataPayload(buffer + hdrlen, packetSize - hdrlen, logFile);
}

/**
 * Parse the data of the packet.
 * 
 * @param buffer     the buffer for receiving data
 * @param packetSize the size of received data
 * @param logFile    the file descriptor of log file
 */
void parseDataPayload(unsigned char* buffer, int packetSize, FILE* logFile) {
    int i = 0, j = 0;

    for ( i = 0; i < packetSize; ++ i ) {
        // If one line of hex printing is complete, the visible characters
        // will be printed here.
        if ( i != 0 && i % CHARS_PER_LINE == 0 ) {
            fprintf(logFile, "         ");

            for ( j = i - CHARS_PER_LINE; j < i; ++ j ) {
                // If it isn't a number or alphabet, print a dot. Otherwise, 
                // print the character.
                if ( buffer[j] >= 32 && buffer[j] <= 128 ) {
                    fprintf(logFile, "%c", (unsigned char) buffer[j]);
                } else {
                    fprintf(logFile, ".");
                }
            }
            fprintf(logFile, "\n");
        }
        
        // Print four spaces at new line
        if ( i % CHARS_PER_LINE == 0 ) {
            fprintf(logFile, "    ");
        }

        // Print the hex charcter in the data.
        fprintf(logFile, "%02X ", (unsigned int) buffer[i]);

        // Print visible characters for the last line
        if ( i == packetSize - 1 ) {
            // Print extra spaces for alignment
            for ( j = 0; j < 15 - i % 16; ++ j ) {
                fprintf(logFile, "   ");
            }
            fprintf(logFile, "         ");

            // Print visible characters on the right
            for ( j = i - i % 16; j < i; ++ j ) {
                if ( buffer[j] >= 32 && buffer[j] <= 128 ) {
                    fprintf(logFile, "%c", (unsigned char) buffer[j]);
                } else {
                    fprintf(logFile, ".");
                }
            }
            // Print \n at the end of data payload
            fprintf(logFile,  "\n");
        }
    }

}