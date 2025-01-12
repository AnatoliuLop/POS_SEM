#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include <time.h>
#include "../include/common.h"
#include "../include/client_api.h"

// Globálne premenne
static int g_my_id = -1;
static int g_my_snake_index = -1;
static int g_paused_client = 0;
static int g_shmid_client = -1;
static GameState *g_game_client = NULL;

static void *thread_input(void *arg);
static void *thread_display(void *arg);

void join_game(int client_id) {
    g_my_id = client_id;

    g_shmid_client = shmget(SHARED_MEMORY_KEY, sizeof(GameState), 0666);
    if (g_shmid_client < 0) {
        printf("[CLIENT] Ziadny server nie je spusteny (shmget zlyhalo).\n");
        return;
    }
    g_game_client = (GameState*) shmat(g_shmid_client, NULL, 0);
    if (g_game_client == (void*)-1) {
        perror("[CLIENT] shmat");
        g_game_client = NULL;
        return;
    }

    pthread_mutex_lock(&game_mutex);

    g_my_snake_index=-1;
    // Nájdem hadíka s player_id
    for (int i=0; i<MAX_PLAYERS; i++) {
        Snake *s = &g_game_client->snakes[i];
        if (s->active && s->player_id==client_id) {
            s->player_connected=1;
            s->paused=0;
            // Zastavíme všetkých hadíkov na 3s
            for (int j=0; j<MAX_PLAYERS; j++) {
                if (g_game_client->snakes[j].active) {
                    g_game_client->snakes[j].paused=1;
                    g_game_client->snakes[j].pause_end_time=time(NULL)+3;
                }
            }
            g_my_snake_index=i;
            break;
        }
    }
    if (g_my_snake_index<0) {
        // Vytvoríme nového
        for (int i=0; i<MAX_PLAYERS; i++) {
            Snake *s = &g_game_client->snakes[i];
            if (!s->active) {
                g_my_snake_index=i;
                s->active=1;
                s->player_id=client_id;
                s->player_connected=1;
                s->paused=0;
                s->pause_end_time=0;
                s->length=4;
                s->direction='d';
                s->score=0;
                if (i==0) {
                    s->body[0].x=2; s->body[0].y=2;
                    s->body[1].x=1; s->body[1].y=2;
                    s->body[2].x=1; s->body[2].y=2;
                    s->body[3].x=1; s->body[3].y=2;
                } else {
                    int w = g_game_client->width;
                    int h = g_game_client->height;
                    if (w<4) w=4;
                    if (h<4) h=4;
                    s->body[0].x=w-3; s->body[0].y=h-3;
                    s->body[1].x=w-4; s->body[1].y=h-3;
                    s->body[2].x=w-4; s->body[2].y=h-3;
                    s->body[3].x=w-4; s->body[3].y=h-3;
                }
                g_game_client->connected_players++;

                for (int j=0; j<MAX_PLAYERS; j++) {
                    if (g_game_client->snakes[j].active) {
                        g_game_client->snakes[j].paused=1;
                        g_game_client->snakes[j].pause_end_time=time(NULL)+3;
                    }
                }
                break;
            }
        }
    }
    pthread_mutex_unlock(&game_mutex);

    if (g_my_snake_index<0) {
        printf("[CLIENT] Hra je plna (ziadne miesto).\n");
        shmdt(g_game_client);
        g_game_client=NULL;
        return;
    }

    pthread_t tin, tout;
    pthread_create(&tin, NULL, thread_input, NULL);
    pthread_create(&tout, NULL, thread_display, NULL);

    pthread_join(tin, NULL);
    pthread_cancel(tout);
    pthread_join(tout, NULL);

    pthread_mutex_lock(&game_mutex);
    if (g_my_snake_index>=0) {
        Snake *s = &g_game_client->snakes[g_my_snake_index];
        if (g_paused_client) {
            // Pauza => hadík ostáva
        } else {
            // Definitívne
            s->active=0;
            g_game_client->connected_players--;
        }
    }
    pthread_mutex_unlock(&game_mutex);

    shmdt(g_game_client);
    g_game_client=NULL;
    g_my_snake_index=-1;
    g_paused_client=0;
}

static void *thread_input(void *arg) {
    (void)arg;
    enable_nonblocking_input();
    while (1) {
        char c = getchar();
        if (c==EOF) {
            usleep(40000);
            continue;
        }
        pthread_mutex_lock(&game_mutex);
        if (!g_game_client || !g_game_client->server_running || g_game_client->game_over) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        if (g_my_snake_index<0) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        Snake *s = &g_game_client->snakes[g_my_snake_index];
        if (!s->active) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        if (c=='p') {
            s->paused=1;
            s->pause_end_time=time(NULL)+9999999;
            g_paused_client=1;
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        char old=s->direction;
        if ((c=='w' && old=='s')||(c=='s' && old=='w')||
            (c=='a' && old=='d')||(c=='d' && old=='a')) {
            // ignoruj
        } else {
            if (c=='w'||c=='a'||c=='s'||c=='d') {
                s->direction=c;
            }
        }
        pthread_mutex_unlock(&game_mutex);
        usleep(50000);
    }
    disable_nonblocking_input();
    return NULL;
}

static void *thread_display(void *arg) {
    (void)arg;
    while (1) {
        pthread_mutex_lock(&game_mutex);
        if (!g_game_client || !g_game_client->server_running || g_game_client->game_over) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        // Clear obrazovku
        printf("\033[H\033[J");

        int elapsed = get_elapsed_seconds(g_game_client->start_time);
        if (g_game_client->mode == GAME_MODE_TIMED) {
            int left = g_game_client->time_limit - elapsed;
            if (left<0) left=0;
            printf("Cas zostava: %d\n", left);
        } else {
            printf("Ubehnuty cas: %d\n", elapsed);
        }

        for (int i=0; i<MAX_PLAYERS; i++) {
            Snake *sn = &g_game_client->snakes[i];
            if (sn->active) {
                printf("Hadik hraca %d => %d bodov\n", sn->player_id, sn->score);
            }
        }

        int H=g_game_client->height;
        int W=g_game_client->width;
        if (H>25) H=25;
        if (W>60) W=60;

        for (int y=0; y<H; y++) {
            for (int x=0; x<W; x++) {
                if (g_game_client->obstacles[y][x] == 1) {
                    putchar('#');
                    continue;
                }
                char ch='.';
                int color_head=0;  // 0=normal, 1=red, 2=green
                // Najprv hladame hady
                for (int p=0; p<MAX_PLAYERS; p++) {
                    Snake *s = &g_game_client->snakes[p];
                    if (!s->active) continue;
                    for (int k=0; k<s->length; k++) {
                        if (s->body[k].x==x && s->body[k].y==y) {
                            if (k==0) {
                                // HLAVA
                                if (p==0) color_head=1; // cervena
                                else if (p==1) color_head=2; // zelena
                                ch='@';
                            } else {
                                ch='o'; 
                            }
                            goto after_find;
                        }
                    }
                }
after_find:;
                if (ch=='.') {
                    // Ovocie 'A'
                    for (int f=0; f<MAX_PLAYERS; f++) {
                        Position *fr = &g_game_client->fruits[f];
                        if (fr->x==x && fr->y==y) {
                            ch='A';
                            break;
                        }
                    }
                }

                if (color_head==1) {
                    // cervena
                    printf("\033[31m%c\033[0m", ch);
                } else if (color_head==2) {
                    // zelena
                    printf("\033[32m%c\033[0m", ch);
                } else {
                    // normal
                    putchar(ch);
                }
            }
            putchar('\n');
        }

        pthread_mutex_unlock(&game_mutex);
        usleep(200000);
    }
    return NULL;
}


