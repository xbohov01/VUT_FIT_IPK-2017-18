CC=gcc
CFLAGS= -Wall -Wextra -std=c99 -pedantic -lm -I.
DEPS_CLIENT = ipk-client.c
DEPS_SERVER = ipk-server.c

all:
	$(CC) $(DEPS_CLIENT) -o ipk-client
	$(CC) $(DEPS_SERVER) -o ipk-server

client:
	$(CC) $(DEPS_CLIENT) -o ipk-client

server:
	$(CC) $(DEPS_SERVER) -o ipk-server

clean:
	rm -f ipk-client ipk-server
