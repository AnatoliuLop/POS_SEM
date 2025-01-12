CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
TARGETS = bin/server bin/client

all: $(TARGETS)

bin/server: src/server.c src/common.c include/common.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/server src/server.c src/common.c

bin/client: src/client.c src/common.c include/common.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/client src/client.c src/common.c -lpthread

clean:
	rm -rf bin

