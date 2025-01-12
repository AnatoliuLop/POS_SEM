CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

INCLUDES = -Iinclude
SRCDIR = src

COMMON_OBJ = $(SRCDIR)/common.o

SERVER_OBJS = $(SRCDIR)/server_main.o \
              $(SRCDIR)/server_game.o \
              $(SRCDIR)/server_ipc.o \
              $(COMMON_OBJ)

CLIENT_OBJS = $(SRCDIR)/client_main.o \
              $(SRCDIR)/client_game.o \
              $(COMMON_OBJ)

.PHONY: all clean

all: server client

server: $(SERVER_OBJS)
	$(CC) $(CFLAGS) -o server $(SERVER_OBJS)

client: $(CLIENT_OBJS)
	$(CC) $(CFLAGS) -o client $(CLIENT_OBJS)

$(SRCDIR)/common.o: $(SRCDIR)/common.c include/common.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCDIR)/common.c -o $(SRCDIR)/common.o

$(SRCDIR)/server_main.o: $(SRCDIR)/server_main.c include/common.h include/server_api.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCDIR)/server_main.c -o $(SRCDIR)/server_main.o

$(SRCDIR)/server_game.o: $(SRCDIR)/server_game.c include/common.h include/server_api.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCDIR)/server_game.c -o $(SRCDIR)/server_game.o

$(SRCDIR)/server_ipc.o: $(SRCDIR)/server_ipc.c include/common.h include/server_api.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCDIR)/server_ipc.c -o $(SRCDIR)/server_ipc.o

$(SRCDIR)/client_main.o: $(SRCDIR)/client_main.c include/common.h include/client_api.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCDIR)/client_main.c -o $(SRCDIR)/client_main.o

$(SRCDIR)/client_game.o: $(SRCDIR)/client_game.c include/common.h include/client_api.h
	$(CC) $(CFLAGS) $(INCLUDES) -c $(SRCDIR)/client_game.c -o $(SRCDIR)/client_game.o

clean:
	rm -f $(SRCDIR)/*.o server client

