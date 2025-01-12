#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <signal.h>
#include "../include/common.h"
#include "../include/server_api.h"

int g_shmid_server = -1;
GameState *g_game_server = NULL;

int create_shared_memory() {
    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), IPC_CREAT | 0666);
    if (shmid<0) {
        perror("shmget");
        exit(1);
    }
    return shmid;
}

GameState* attach_shared_memory(int shmid) {
    GameState *g = (GameState*) shmat(shmid, NULL, 0);
    if ((void*)g == (void*)-1) {
        perror("shmat");
        exit(1);
    }
    return g;
}

void remove_shared_memory(int shmid, GameState *g) {
    if (g) {
        shmdt(g);
    }
    if (shmid>=0) {
        shmctl(shmid, IPC_RMID, NULL);
    }
}

