#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/common.h"

void generate_fruit(GameState *game) {
    int fruit_placed = 0;
    while (!fruit_placed) {
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;
        if (game->grid[y][x] == 0) {
            game->grid[y][x] = 3; // Обозначение фрукта
            game->fruit_x = x;
            game->fruit_y = y;
            fruit_placed = 1;
        }
    }
}

void initialize_game(GameState *game) {
    pthread_mutex_lock(&game_mutex);

    game->player_count = 0;
    game->ready_players = 0;
    game->game_over = 0;
    game->server_running = 1; // Сервер активен

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            game->grid[y][x] = 0;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i < game->player_count) { // Только для активных игроков
            game->players[i].length = 1;
            game->players[i].score = 0;
            game->players[i].direction = 'd';
            game->players[i].active = 1;
            game->players[i].snake[0].x = (i == 0) ? 1 : GRID_SIZE - 2;
            game->players[i].snake[0].y = (i == 0) ? 1 : GRID_SIZE - 2;
            game->grid[game->players[i].snake[0].y][game->players[i].snake[0].x] = i + 1;
        } else {
            game->players[i].active = 0; // Неактивные игроки
        }
    }

    generate_fruit(game);
    pthread_mutex_unlock(&game_mutex);
}


void *game_loop(void *arg) {
    GameState *game = (GameState *)arg;
  printf("BEFORE WHILE IN GAME LOOP \n");
 printf("[GAME_LOOP] Exiting game loop. game->game_over: %d, game->server_running: %d\n", game->game_over, game->server_running);
    while (game->server_running && !game->game_over) {
    printf("I`m in WHILE BEFORE MUTEX\n");
    printf("\n");
        pthread_mutex_lock(&game_mutex);
printf("\n");
printf("I`m in WHILE AFTER MUTEX\n");
    printf("\n");
        for (int i = 0; i < game->player_count; i++) {
            PlayerData *player = &game->players[i];
            if (player->active) {
                // Проверка, было ли обновлено направление
                if (player->direction != 0) {
                    printf("[SERVER] Received from Player %d: %c\n", i, player->direction);

                    Position head = player->snake[0];
                    Position new_head = head;

                    switch (player->direction) {
                        case 'w': new_head.y--; break;
                        case 's': new_head.y++; break;
                        case 'a': new_head.x--; break;
                        case 'd': new_head.x++; break;
                    }

                    // Перемещение змейки
                    for (int j = player->length - 1; j > 0; j--) {
                        player->snake[j] = player->snake[j - 1];
                    }
                    player->snake[0] = new_head;
                    game->grid[new_head.y][new_head.x] = i + 1;

                    // Сбрасываем направление после обработки
                    player->direction = 0;
                }else {
                    printf("[SERVER] Player %d direction is still 0 (no input received).\n", i);
                }
            }
        }

        pthread_mutex_unlock(&game_mutex);
        usleep(200000);
    }
  printf("\n");
  printf("AFTER WHILE IN GAME LOOP\n");
 printf("[GAME_LOOP] Exiting game loop. game->game_over: %d, game->server_running: %d\n", game->game_over, game->server_running);
    return NULL;
}
// Функция для завершения сервера и очистки памяти
void cleanup_and_exit(int shmid, GameState *game) {
    printf("Cleaning up and exiting...\n");

    pthread_mutex_lock(&game_mutex);
    game->server_running = 0; // Сервер остановлен
    pthread_mutex_unlock(&game_mutex);

    if (game != (GameState *)-1) {
        shmdt((void *)game);
    }
    if (shmid >= 0) {
        shmctl(shmid, IPC_RMID, NULL);
    }
    exit(0);
}


// Поток для обработки клавиши P
void *input_thread(void *arg) {
    int shmid = *((int *)arg);
    GameState *game = (GameState *)shmat(shmid, NULL, 0);

    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }

    while (1) {
        char input = getchar();
        if (input == 'P' || input == 'p') {
            cleanup_and_exit(shmid, game);
        }
    }
}

int main() {
    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    GameState *game = (GameState *)shmat(shmid, NULL, 0);
    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }
printf("Shared memory created. SHMID: %d\n", shmid);

    initialize_game(game);

    printf("Server started...\n");
    printf("Select number of players (1 or 2): ");
    scanf("%d", &game->player_count);

    printf("Waiting for players to connect...\n");
    while (game->ready_players < game->player_count) {
        pthread_mutex_lock(&game_mutex);
        printf("Players connected: %d/%d\n", game->ready_players, game->player_count);
        pthread_mutex_unlock(&game_mutex);
        usleep(500000);
    }

    printf("All players connected! Starting the game...\n");

    pthread_t game_thread, input_thread_handle;
    pthread_create(&game_thread, NULL, game_loop, (void *)game);
   printf("[SERVER] Entering game loop...\n");
    pthread_create(&input_thread_handle, NULL, input_thread, (void *)&shmid);
   printf("[SERVER] Game loop exited. Cleaning up...\n");

    pthread_join(game_thread, NULL);
    pthread_join(input_thread_handle, NULL);

    cleanup_and_exit(shmid, game);

    return 0;
}

