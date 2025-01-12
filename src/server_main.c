#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "../include/common.h"
#include "../include/server_api.h"

extern int g_shmid_server;
extern GameState *g_game_server;

// Из server_ipc.c
int create_shared_memory();
GameState* attach_shared_memory(int shmid);
void remove_shared_memory(int shmid, GameState *game);

pthread_t g_server_game_thread;

int main()
{
    printf("=== Server Startup ===\n");
    int w, h, mode_int, tlimit, wtype_int;
    printf("Width (max 100)? ");
    scanf("%d", &w);
    printf("Height (max 100)? ");
    scanf("%d", &h);
    printf("Game mode? (0=Standard, 1=Timed): ");
    scanf("%d", &mode_int);
    tlimit=0;
    if (mode_int==1) {
        printf("Time limit (sec)? ");
        scanf("%d", &tlimit);
    }
    printf("World type? (0=No obstacles, 1=With obstacles): ");
    scanf("%d", &wtype_int);

    g_shmid_server = create_shared_memory();
    g_game_server  = attach_shared_memory(g_shmid_server);

    pthread_mutex_lock(&game_mutex);
    server_init_game(g_game_server, w, h,
                     (GameMode)mode_int,
                     tlimit,
                     (WorldType)wtype_int);
    pthread_mutex_unlock(&game_mutex);

    signal(SIGINT, server_stop);

    // Поток цикла
    pthread_create(&g_server_game_thread, NULL,
                   server_game_loop_thread,
                   (void*)g_game_server);

    pthread_join(g_server_game_thread, NULL);

    pthread_mutex_lock(&game_mutex);
    printf("[SERVER] Cleaning up...\n");
    pthread_mutex_unlock(&game_mutex);

    remove_shared_memory(g_shmid_server, g_game_server);
    return 0;
}

