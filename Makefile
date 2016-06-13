CC=gcc
CFLAGS=-I -Wall.

server: server.c
	$(CC) -o server server.c $(CFLAGS)


clean:
	rm -f ./*.o server