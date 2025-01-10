#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <termios.h>
#include <fcntl.h>
#include "../include/common.h"

// Включение неблокирующего ввода
void enable_nonblocking_input() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL) | O_NONBLOCK);
}

// Отключение неблокирующего ввода
void disable_nonblocking_input() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= ICANON | ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

int main() {
    printf("Client started...\n");

    // Подключение к общей памяти
    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    GameState *game = (GameState *)shmat(shmid, NULL, 0);
    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }

    // Определение ID игрока
    int player_id = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!game->players[i].active) {
            player_id = i;
            break;
        }
    }

    if (player_id == -1) {
        printf("No available player slots. Exiting.\n");
        shmdt((void *)game);
        exit(1);
    }

    // Установка флага активности игрока и увеличения ready_players
    game->players[player_id].active = 1;
    __sync_fetch_and_add(&game->ready_players, 1); // Атомарное увеличение ready_players
    printf("Player %d connected and ready!\n", player_id);

    enable_nonblocking_input(); // Включаем неблокирующий ввод

    while (!game->game_over) {
        char command = getchar();
        if ((command == 'w' && game->players[player_id].direction != 's') || // Запрет движения вниз
            (command == 's' && game->players[player_id].direction != 'w') || // Запрет движения вверх
            (command == 'a' && game->players[player_id].direction != 'd') || // Запрет движения вправо
            (command == 'd' && game->players[player_id].direction != 'a')) { // Запрет движения влево
            game->players[player_id].direction = command; // Меняем направление
        } else if (command == 'q') {
            break; // Выход из игры
        }
        usleep(200000); // Задержка для плавности
    }

    printf("Game over! Your final score: %d\n", game->players[player_id].score);

    disable_nonblocking_input(); // Отключаем неблокирующий ввод
    shmdt((void *)game);         // Отключение от общей памяти
    printf("Client stopped.\n");
    return 0;
}

