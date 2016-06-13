CC=gcc
CFLAGS=-Wall -I.

all: server tcp-client udp-client

server: server.c
	$(CC) -o server server.c $(CFLAGS)

tcp-client: tcp-client.c
	$(CC) -o tcp-client tcp-client.c $(CFLAGS)

udp-client: udp-client.c
	$(CC) -o udp-client udp-client.c $(CFLAGS)

clean:
	rm -f ./*.o server tcp-client udp-client