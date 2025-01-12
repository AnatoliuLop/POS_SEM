#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/common.h"

void *handle_input(void *arg) {
    GameState *game = (GameState *)arg;
    int player_id = game->ready_players - 1;

    while (!game->game_over) {
        char input = getchar();
        if (input == 'w' || input == 'a' || input == 's' || input == 'd') {
            pthread_mutex_lock(&game_mutex);
            game->players[player_id].direction = input;
            pthread_mutex_unlock(&game_mutex);
        }
    }

    return NULL;
}

void *handle_display(void *arg) {
    GameState *game = (GameState *)arg;

    while (!game->game_over) {
        system("clear");
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
        usleep(100000);
    }

    return NULL;
}

int main() {
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

    pthread_mutex_lock(&game_mutex);
    game->ready_players++;
    pthread_mutex_unlock(&game_mutex);

    pthread_t input_thread, display_thread;
    pthread_create(&input_thread, NULL, handle_input, (void *)game);
    pthread_create(&display_thread, NULL, handle_display, (void *)game);

    pthread_join(input_thread, NULL);
    pthread_join(display_thread, NULL);

    shmdt((void *)game);

    return 0;
}

