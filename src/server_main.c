#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "../include/common.h"
#include "../include/server_api.h"

// Globálne
extern int g_shmid_server;
extern GameState *g_game_server;

// Deklarácie z server_ipc.c
int create_shared_memory();
GameState* attach_shared_memory(int shmid);
void remove_shared_memory(int shmid, GameState *g);

pthread_t g_server_game_thread;

// ./server  <width> <height> <mode> <time_limit> <wtype>
int main(int argc, char *argv[])
{
    if (argc < 5) {
        fprintf(stderr, "Pouzitie: %s <width> <height> <mode> <time_limit> <wtype>\n", argv[0]);
        fprintf(stderr, "Priklad: %s 20 20 0 0 1\n", argv[0]);
        return 1;
    }

    int w = atoi(argv[1]);
    int h = atoi(argv[2]);
    int mode_int = atoi(argv[3]);
    int tlimit = atoi(argv[4]);
    int wtype_int = 0;
    if (argc>=6) {
        wtype_int = atoi(argv[5]);
    }

    printf("[SERVER] Spustam s parametrami: w=%d, h=%d, mode=%d, tlimit=%d, wtype=%d\n",
           w,h,mode_int,tlimit,wtype_int);

    g_shmid_server = create_shared_memory();
    g_game_server  = attach_shared_memory(g_shmid_server);

    pthread_mutex_lock(&game_mutex);
    server_init_game(g_game_server, w, h, (GameMode)mode_int, tlimit, (WorldType)wtype_int, MAX_PLAYERS);
    pthread_mutex_unlock(&game_mutex);

    signal(SIGINT, server_stop);

    pthread_create(&g_server_game_thread, NULL, server_game_loop_thread, (void*)g_game_server);
    pthread_join(g_server_game_thread, NULL);

    pthread_mutex_lock(&game_mutex);
    printf("[SERVER] Koncim, cistim zdroje.\n");
    pthread_mutex_unlock(&game_mutex);

    remove_shared_memory(g_shmid_server, g_game_server);
    return 0;
}

