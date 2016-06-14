# Multiplex-Socket

## Introduction

This is the homework of *Advanced Programming for Internet* in [Harbin Institute of Technology](http://www.hit.edu.cn).

The project contains two projects:

### Multiplex Server

A TCP and UDP multiplex service for file transfer service using select.

The server can accept both TCP and UDP connections [Learn More](http://www.binarytides.com/packet-sniffer-code-c-linux/).

### Packet Sniffer

Packet sniffers that intercept the network traffic flowing in and out of a system through network interfaces.

The sniffer uses a raw socket when put in recvfrom loop receives all incoming packets.

## Setup

> Note: You need to compile this project in Linux or Unix (including Mac OS X).

### Compile

You can simply compile this project use `make` command.

### Run Multiplex Server

After the compile operation is successful, you can run the server:

```
./server <PortNumber>
```

Then you can start a UDP client or TCP client:

```
# for TCP client
./tcp-client <ServerIP> <PortNumber>

# for UCP client
./udp-client <ServerIP> <PortNumber>
```

In TCP client, you can get files from the server:

```
GET <Path to the file in server>
```

**Known issues:** 

- Segment fault will be raised if the path is invalid or the file doesn't exist in server.
- Segment fault will be caused if you have no previlige to save the file in client.
- The client may be blocked for unknown reason while transfering files.

### Run Packet Sniffers

> NOTE: In Linux/Unix systems, you need root permissions to receive raw packets on an interface. This restriction is a security precaution, because a process that receives raw packets gains access to communications of all other processes and users using that interface.

```
sudo ./packet-sniffer
```

All incoming packets will dumped into `packet-sniffer.log` file.

## License

This project is open sourced under Apache license.