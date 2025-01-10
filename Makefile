CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude

TARGETS = server client

all: $(TARGETS)

server: src/server.c
	$(CC) $(CFLAGS) -o server src/server.c

client: src/client.c
	$(CC) $(CFLAGS) -o client src/client.c

clean:
	rm -f $(TARGETS)

