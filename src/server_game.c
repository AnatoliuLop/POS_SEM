#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include "../include/common.h"
#include "../include/server_api.h"

extern int g_shmid_server;
extern GameState *g_game_server;

// clamp_dimension
static int clamp_dimension(int val) {
    if (val < 1)  return 1;
    if (val > 100) return 100;
    return val;
}

void server_init_game(GameState *game, int width, int height,
                      GameMode mode, int time_limit, WorldType wtype,
                      int max_players)
{
    (void)max_players; // nevyužité

    width = clamp_dimension(width);
    height = clamp_dimension(height);

    memset(game, 0, sizeof(GameState));
    game->width = width;
    game->height = height;
    game->mode = mode;
    game->time_limit = time_limit;
    game->wtype = wtype;
    game->start_time = time(NULL);
    game->server_running = 1;

    memset(game->obstacles, 0, sizeof(game->obstacles));
    if (wtype == WORLD_WITH_OBSTACLES) {
        for (int x=0; x<width; x++) {
            game->obstacles[0][x] = 1;
            game->obstacles[height-1][x] = 1;
        }
        for (int y=0; y<height; y++) {
            game->obstacles[y][0] = 1;
            game->obstacles[y][width-1] = 1;
        }
    }

    printf("[SERVER] Inicializujem hru %dx%d, rezim=%s, prekazky=%s\n",
        width, height,
        (mode==GAME_MODE_STANDARD)?"STANDARD":"TIMED",
        (wtype==WORLD_WITH_OBSTACLES)?"ANO":"NIE");
}

void *server_game_loop_thread(void *arg) {
    GameState *game = (GameState*)arg;

    while (1) {
        pthread_mutex_lock(&game_mutex);

        if (!game->server_running) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        if (game->game_over) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        // Časový režim
        if (game->mode == GAME_MODE_TIMED) {
            int elapsed = get_elapsed_seconds(game->start_time);
            if (elapsed >= game->time_limit) {
                printf("[SERVER] Cas vyprsal - KONIEC HRY.\n");
                game->game_over = 1;
            }
        }

        // Aktívne hady
        int active_count=0;
        for (int i=0; i<MAX_PLAYERS; i++) {
            if (game->snakes[i].active) active_count++;
        }

        if (active_count==0) {
            if (game->mode == GAME_MODE_STANDARD) {
                if (game->last_snake_left_time==0) {
                    game->last_snake_left_time = time(NULL);
                } else {
                    int diff = get_elapsed_seconds(game->last_snake_left_time);
                    if (diff >= 10) {
                        printf("[SERVER] 10s bez hracov -> KONIEC HRY.\n");
                        game->game_over = 1;
                    }
                }
            }
        } else {
            game->last_snake_left_time=0;
        }

        server_generate_fruits(game);

        for (int i=0; i<MAX_PLAYERS; i++) {
            Snake *s = &game->snakes[i];
            if (!s->active) continue;

            time_t now = time(NULL);
            if (s->paused) {
                if (now >= s->pause_end_time) {
                    s->paused=0;
                } else {
                    continue;
                }
            }

            if (s->direction!=0) {
                server_move_snake(game, i);
                server_check_collisions(game, i);
            }
        }

        pthread_mutex_unlock(&game_mutex);
        usleep(120000);
    }

    pthread_mutex_lock(&game_mutex);
    printf("\n[SERVER] Hra skoncila, skore:\n");
    for (int i=0; i<MAX_PLAYERS; i++) {
        Snake *s = &game->snakes[i];
        printf(" - Hadik hraca %d => %d bodov\n", s->player_id, s->score);
    }
    game->server_running=0;
    pthread_mutex_unlock(&game_mutex);

    return NULL;
}

// Wrap-around
static void wrap_around(GameState *game, Position *pos) {
    if (pos->x < 0) pos->x = game->width-1;
    if (pos->x >= game->width) pos->x = 0;
    if (pos->y < 0) pos->y = game->height-1;
    if (pos->y >= game->height) pos->y = 0;
}

void server_move_snake(GameState *game, int i)
{
    Snake *s = &game->snakes[i];
    if (!s->active) return;
    if (s->direction==0) return;

    Position head = s->body[0];
    Position new_head = head;

    switch(s->direction) {
        case 'w': new_head.y--; break;
        case 's': new_head.y++; break;
        case 'a': new_head.x--; break;
        case 'd': new_head.x++; break;
        default: break;
    }

    if (game->wtype == WORLD_NO_OBSTACLES) {
        wrap_around(game, &new_head);
    }

    for (int k=s->length-1; k>0; k--) {
        s->body[k] = s->body[k-1];
    }
    s->body[0] = new_head;
}

void server_check_collisions(GameState *game, int i)
{
    Snake *s = &game->snakes[i];
    if (!s->active) return;
    Position head = s->body[0];

    if (game->wtype == WORLD_WITH_OBSTACLES) {
        if (head.x<0 || head.x>=game->width || head.y<0 || head.y>=game->height) {
            printf("[SERVER] Hadik hraca %d narazil do okraja.\n", s->player_id);
            s->active=0;
            game->connected_players--;
            return;
        }
    }
    if (head.x>=0 && head.x<game->width &&
        head.y>=0 && head.y<game->height)
    {
        if (game->obstacles[head.y][head.x] == 1) {
            printf("[SERVER] Hadik hraca %d narazil do prekazky.\n", s->player_id);
            s->active=0;
            game->connected_players--;
            return;
        }
    }

    // Seba
    for (int k=1; k<s->length; k++) {
        if (s->body[k].x==head.x && s->body[k].y==head.y) {
            printf("[SERVER] Hadik hraca %d narazil do seba sameho.\n", s->player_id);
            s->active=0;
            game->connected_players--;
            return;
        }
    }

    // Iny had
    for (int p=0; p<MAX_PLAYERS; p++) {
        if (p==i) continue;
        Snake *o=&game->snakes[p];
        if (!o->active) continue;
        for (int k=0; k<o->length; k++) {
            if (o->body[k].x==head.x && o->body[k].y==head.y) {
                printf("[SERVER] Hadik hraca %d narazil do hadika hraca %d.\n",
                       s->player_id, o->player_id);
                s->active=0;
                game->connected_players--;
                return;
            }
        }
    }

    // Ovocie 'A'
    for (int f=0; f<MAX_PLAYERS; f++) {
        if (head.x==game->fruits[f].x && head.y==game->fruits[f].y) {
            s->score+=10;
            if (s->length<MAX_SNAKE_LENGTH) {
                s->length++;
                s->body[s->length-1] = s->body[s->length-2];
            }
            game->fruits[f].x=-1;
            game->fruits[f].y=-1;
            break;
        }
    }
}

void server_generate_fruits(GameState *game)
{
    int active_snakes=0;
    for (int i=0; i<MAX_PLAYERS; i++) {
        if (game->snakes[i].active) active_snakes++;
    }

    int fruit_count=0;
    for (int f=0; f<MAX_PLAYERS; f++) {
        if (game->fruits[f].x>=0 && game->fruits[f].y>=0) fruit_count++;
    }

    while (fruit_count<active_snakes) {
        int slot=-1;
        for (int f=0; f<MAX_PLAYERS; f++) {
            if (game->fruits[f].x<0) { slot=f; break;}
        }
        if (slot<0) break;

        while(1) {
            int rx=rand()%game->width;
            int ry=rand()%game->height;
            if (game->obstacles[ry][rx]==1) continue;
            int used=0;
            for (int i=0; i<MAX_PLAYERS; i++) {
                if (!game->snakes[i].active) continue;
                for (int k=0; k<game->snakes[i].length; k++) {
                    if (game->snakes[i].body[k].x==rx && 
                        game->snakes[i].body[k].y==ry) {
                        used=1; 
                        break;
                    }
                }
                if (used) break;
            }
            if (!used) {
                game->fruits[slot].x=rx;
                game->fruits[slot].y=ry;
                fruit_count++;
                break;
            }
        }
    }

    while (fruit_count>active_snakes) {
        for (int f=0; f<MAX_PLAYERS; f++) {
            if (game->fruits[f].x>=0) {
                game->fruits[f].x=-1;
                game->fruits[f].y=-1;
                fruit_count--;
                break;
            }
        }
    }
}

void server_stop(int signo) {
    (void)signo;
    if (g_game_server) {
        pthread_mutex_lock(&game_mutex);
        g_game_server->game_over=1;
        pthread_mutex_unlock(&game_mutex);
    }
}

void server_cleanup_and_exit() {
    // Nic
}

