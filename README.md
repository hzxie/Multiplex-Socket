# Multiplex-Socket

## Introduction

A TCP and UDP multiplex service for file transfer service using select.

The server can accept both TCP and UDP connections.

This is the homework of *Advanced Programming for Internet* in [Harbin Institute of Technology](http://www.hit.edu.cn).

## Setup

> Note: You need to compile this project in Linux or Unix (including Mac OS X).

### Compile

You can simply compile this project use `make` command.

### Run

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

Enjoy!

## License

This project is open sourced under Apache license.