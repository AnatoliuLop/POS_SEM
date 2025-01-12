#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/common.h"

// Поток обработки ввода от пользователя
void *handle_input(void *arg) {
    GameState *game = (GameState *)arg;
    int player_id = game->ready_players - 1;

    while (!game->game_over) {
        char input = getchar();
        if (input == 'w' || input == 'a' || input == 's' || input == 'd') {
            printf("[CLIENT] Preparing to send: Player %d pressed %c\n", player_id, input);
            pthread_mutex_lock(&game_mutex);
            game->players[player_id].direction = input;
            pthread_mutex_unlock(&game_mutex);

            // Логирование отправки
            printf("Sent to server: Player %d pressed %c\n", player_id + 1, input);
        }
      usleep(50000); // Короткая задержка, чтобы не загружать процессор
    }

    return NULL;
}


// Поток обработки отображения игрового поля
void *handle_display(void *arg) {
    GameState *game = (GameState *)arg;

    while (!game->game_over) {
        pthread_mutex_lock(&game_mutex);

        // Проверяем, подключились ли все игроки
        if (game->ready_players < game->player_count) {
            printf("Waiting for other players to connect...\n");
            pthread_mutex_unlock(&game_mutex);
            usleep(500000); // Полсекунды ожидания
            continue;
        }

        // Проверяем, если сервер завершил игру
        if (!game->server_running) {
            printf("SERVER STOPPED THE GAME!\n");
            pthread_mutex_unlock(&game_mutex);
            exit(0);
        }

        // Отображаем игровое поле
        // system("clear");
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                if (game->grid[y][x] == 0) printf(".");
                else if (game->grid[y][x] == 1) printf("O");
                else if (game->grid[y][x] == 2) printf("G");
                else if (game->grid[y][x] == 3) printf("F");
            }
            printf("\n");
        }
        printf("Your score: %d\n", game->players[game->ready_players - 1].score);

        pthread_mutex_unlock(&game_mutex);
        usleep(100000); // Задержка для плавности отображения
    }

    return NULL;
}

int main() {
    // Подключение к разделяемой памяти
printf("Trying to connect to shared memory...\n");

int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), 0666);
if (shmid < 0) {
    perror("shmget");
    exit(1);
}

printf("Connected to shared memory. SHMID: %d\n", shmid);
    GameState *game = (GameState *)shmat(shmid, NULL, 0);
    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }

    // Проверка, можно ли подключиться
    pthread_mutex_lock(&game_mutex);
    if (game->ready_players >= game->player_count) {
        printf("Error: Game is already full!\n");
        pthread_mutex_unlock(&game_mutex);
        shmdt((void *)game);
        exit(1);
    }
    game->ready_players++;
    pthread_mutex_unlock(&game_mutex);

    printf("Client started...\n");
    printf("Player %d connected and ready!\n", game->ready_players - 1);
    printf("\n");
    printf("\n");
    printf("\n");
    // Создание потоков для ввода и отображения
      enable_nonblocking_input();

    pthread_t input_thread, display_thread;
    pthread_create(&input_thread, NULL, handle_input, (void *)game);
    pthread_create(&display_thread, NULL, handle_display, (void *)game);

    pthread_join(input_thread, NULL);
    pthread_join(display_thread, NULL);

     disable_nonblocking_input(); // Отключение неблокирующего ввода

    // Отсоединение от разделяемой памяти
    shmdt((void *)game);

    return 0;
}

