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

    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            game->grid[y][x] = 0;
        }
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->players[i].length = 1;
        game->players[i].score = 0;
        game->players[i].direction = 'd';
        game->players[i].active = 0;
        game->players[i].snake[0].x = (i == 0) ? 1 : GRID_SIZE - 2;
        game->players[i].snake[0].y = (i == 0) ? 1 : GRID_SIZE - 2;
        game->grid[game->players[i].snake[0].y][game->players[i].snake[0].x] = i + 1;
    }

    generate_fruit(game);
    pthread_mutex_unlock(&game_mutex);
}

void *game_loop(void *arg) {
    GameState *game = (GameState *)arg;

    while (!game->game_over) {
        pthread_mutex_lock(&game_mutex);

        for (int i = 0; i < game->player_count; i++) {
            PlayerData *player = &game->players[i];
            if (player->active) {
                Position head = player->snake[0];
                Position new_head = head;

                switch (player->direction) {
                    case 'w': new_head.y--; break;
                    case 's': new_head.y++; break;
                    case 'a': new_head.x--; break;
                    case 'd': new_head.x++; break;
                }

                // Проверка выхода за границы поля
             if (new_head.x < 0 || new_head.x >= GRID_SIZE || 
                 new_head.y < 0 || new_head.y >= GRID_SIZE || 
                (game->grid[new_head.y][new_head.x] != 0 && game->grid[new_head.y][new_head.x] != 3)) {
                    game->game_over = 1;
                    break;
                }

                // Проверка на фрукт
                if (game->grid[new_head.y][new_head.x] == 3) {
                    player->score++;
                    player->length++;
                    generate_fruit(game);
                }

                // Перемещение змейки
                for (int j = player->length - 1; j > 0; j--) {
                    player->snake[j] = player->snake[j - 1];
                }
                player->snake[0] = new_head;
                game->grid[new_head.y][new_head.x] = i + 1;
            }
        }

        pthread_mutex_unlock(&game_mutex);
        usleep(200000);
    }

    return NULL;
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

    initialize_game(game);

    printf("Server started...\n");
    printf("Select number of players (1 or 2): ");
    scanf("%d", &game->player_count);

    printf("Waiting for players to connect...\n");
    while (game->ready_players < game->player_count);

    printf("All players connected! Starting the game...\n");

    pthread_t game_thread;
    pthread_create(&game_thread, NULL, game_loop, (void *)game);
    pthread_join(game_thread, NULL);

    shmdt((void *)game);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

