# Компилятор и флаги
CC = gcc
CFLAGS = -Wall -Wextra -g -Iinclude
TARGETS = bin/server bin/client

# Цели по умолчанию
all: $(TARGETS)

# Компиляция сервера
bin/server: src/server.c include/common.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/server src/server.c

# Компиляция клиента
bin/client: src/client.c include/common.h
	mkdir -p bin
	$(CC) $(CFLAGS) -o bin/client src/client.c

# Удаление скомпилированных файлов
clean:
	rm -rf bin

