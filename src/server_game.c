#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include "../include/common.h"
#include "../include/server_api.h"

extern int g_shmid_server;
extern GameState *g_game_server;

void server_init_game(GameState *game, int width, int height,
                      GameMode mode, int time_limit, WorldType wtype)
{
    // CHANGED: жёстко ограничиваем размер поля 100x100
    if (width  > MAX_WIDTH)  width  = MAX_WIDTH;
    if (height > MAX_HEIGHT) height = MAX_HEIGHT;

    memset(game, 0, sizeof(GameState));
    game->width = width;
    game->height = height;
    game->mode = mode;
    game->time_limit = time_limit;
    game->wtype = wtype;
    game->start_time = time(NULL);
    game->server_running = 1;

    // Заполним obstacles нулями
    memset(game->obstacles, 0, sizeof(game->obstacles));

    // Если wtype=WITH_OBSTACLES, делаем рамку
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

    printf("[SERVER] Game inited: %dx%d, mode=%s, obstacles=%s\n",
        width, height,
        (mode==GAME_MODE_STANDARD)?"standard":"timed",
        (wtype==WORLD_WITH_OBSTACLES)?"YES":"NO");
}

void *server_game_loop_thread(void *arg) {
    GameState *game = (GameState*) arg;

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

        // Если TIMED — проверяем время
        if (game->mode == GAME_MODE_TIMED) {
            int elapsed = get_elapsed_seconds(game->start_time);
            if (elapsed >= game->time_limit) {
                printf("[SERVER] Time is up => GAME OVER.\n");
                game->game_over = 1;
            }
        }

        // Сколько змей реально "живы"
        int active_count = 0;
        for (int i=0; i<MAX_PLAYERS; i++) {
            if (game->snakes[i].active) active_count++;
        }

        // Сколько подключённых клиентов
        if (active_count==0) {
            // Никто не играет (все умерли или вышли), ждем 10 сек (standard)
            if (game->mode == GAME_MODE_STANDARD) {
                if (game->last_snake_left_time == 0) {
                    game->last_snake_left_time = time(NULL);
                } else {
                    int diff = (int)(time(NULL) - game->last_snake_left_time);
                    if (diff >= 10) {
                        printf("[SERVER] 10s no players => GAME OVER.\n");
                        game->game_over = 1;
                    }
                }
            }
        } else {
            game->last_snake_left_time = 0;
        }

        // Генерация фруктов
        server_generate_fruits(game);

        // Двигаем каждую змею
        for (int i=0; i<MAX_PLAYERS; i++) {
            Snake *s = &game->snakes[i];
            if (!s->active) continue;

            // CHANGED: если player_connected=0, значит клиент «вышел», 
            // мы НЕ обновляем направление (не меняется), 
            // но змея всё равно двигается (если direction != 0), 
            // Если хотите, чтобы она стояла на месте, обнуляйте direction 
            // в момент выхода. (Но ниже сделаем в client_game.c)
            server_move_snake(game, i);
            server_check_collisions(game, i);
        }

        pthread_mutex_unlock(&game_mutex);
        usleep(150000);
    }

    // Игра кончена
    pthread_mutex_lock(&game_mutex);
    printf("\n[SERVER] GAME OVER.\n\n");
    printf("Final scores:\n");
    for (int i=0; i<MAX_PLAYERS; i++) {
        if (game->snakes[i].active) {
            printf(" - Snake of player_id=%d => %d points\n", 
                   game->snakes[i].player_id, 
                   game->snakes[i].score);
        }
        else {
            // Даже если неактивна, можно показать
            // (на вкус - как хотите)
            printf(" - Snake of player_id=%d => %d points\n",
                   game->snakes[i].player_id,
                   game->snakes[i].score);
        }
    }
    game->server_running = 0;
    pthread_mutex_unlock(&game_mutex);

    return NULL;
}

// Запрет поворота на 180°
static int is_opposite_direction(char d1, char d2) {
    if ((d1=='w' && d2=='s') || (d1=='s' && d2=='w')) return 1;
    if ((d1=='a' && d2=='d') || (d1=='d' && d2=='a')) return 1;
    return 0;
}

void server_move_snake(GameState *game, int i)
{
    Snake *s = &game->snakes[i];
    if (!s->active) return;
    if (s->direction == 0) return;

    Position head = s->body[0];
    Position new_head = head;

    switch(s->direction) {
        case 'w': new_head.y--; break;
        case 's': new_head.y++; break;
        case 'a': new_head.x--; break;
        case 'd': new_head.x++; break;
        default: break;
    }

    // Мир c wrap-around
    if (game->wtype == WORLD_NO_OBSTACLES) {
        if (new_head.x < 0) new_head.x = game->width - 1;
        if (new_head.x >= game->width) new_head.x = 0;
        if (new_head.y < 0) new_head.y = game->height - 1;
        if (new_head.y >= game->height) new_head.y = 0;
    }
    // Мир с препятствиями => проверим выход из поля в check_collisions()

    // Сдвигаем тело
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

    // Выход из поля
    if (game->wtype == WORLD_WITH_OBSTACLES) {
        if (head.x<0 || head.x>=game->width ||
            head.y<0 || head.y>=game->height) {
            printf("[SERVER] Snake of player_id=%d hit border.\n", s->player_id);
            s->active = 0;
            game->connected_players--;
            return;
        }
    }

    // Препятствия
    if (head.y>=0 && head.y<game->height &&
        head.x>=0 && head.x<game->width)
    {
        if (game->obstacles[head.y][head.x] == 1) {
            printf("[SERVER] Snake of player_id=%d hit obstacle!\n", s->player_id);
            s->active = 0;
            game->connected_players--;
            return;
        }
    }

    // Самопересечение
    for (int k=1; k<s->length; k++) {
        if (s->body[k].x==head.x && s->body[k].y==head.y) {
            printf("[SERVER] Snake of player_id=%d collided with itself!\n", s->player_id);
            s->active = 0;
            game->connected_players--;
            return;
        }
    }

    // Столкновение с другими
    for (int p=0; p<MAX_PLAYERS; p++) {
        if (p==i) continue;
        if (!game->snakes[p].active) continue;
        for (int k=0; k<game->snakes[p].length; k++) {
            if (game->snakes[p].body[k].x==head.x &&
                game->snakes[p].body[k].y==head.y)
            {
                printf("[SERVER] Snake of player_id=%d collided with snake of player_id=%d!\n",
                       s->player_id, game->snakes[p].player_id);
                s->active = 0;
                game->connected_players--;
                return;
            }
        }
    }

    // Фрукты
    for (int f=0; f<MAX_PLAYERS; f++) {
        if (head.x==game->fruits[f].x && head.y==game->fruits[f].y) {
            s->score += 10;
            // растём
            if (s->length<MAX_SNAKE_LENGTH) {
                s->length++;
                s->body[s->length-1] = s->body[s->length-2];
            }
            // убираем фрукт
            game->fruits[f].x=-1;
            game->fruits[f].y=-1;
            break;
        }
    }
}

void server_generate_fruits(GameState *game)
{
    // считаем имеющиеся фрукты
    int fruit_count=0;
    for (int f=0; f<MAX_PLAYERS; f++) {
        if (game->fruits[f].x>=0 && game->fruits[f].y>=0) {
            fruit_count++;
        }
    }
    // считаем активных змей
    int snake_count=0;
    for (int i=0; i<MAX_PLAYERS; i++) {
        if (game->snakes[i].active) snake_count++;
    }

    while (fruit_count<snake_count) {
        int slot=-1;
        for (int f=0; f<MAX_PLAYERS; f++) {
            if (game->fruits[f].x<0) {slot=f; break;}
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
                        used=1; break;
                    }
                }
                if (used) break;
            }
            if (used) continue;

            // ставим
            game->fruits[slot].x=rx;
            game->fruits[slot].y=ry;
            fruit_count++;
            break;
        }
    }

    while (fruit_count>snake_count) {
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

void server_stop(int signo)
{
    if (g_game_server) {
        pthread_mutex_lock(&game_mutex);
        g_game_server->game_over=1;
        pthread_mutex_unlock(&game_mutex);
    }
}

void server_cleanup_and_exit()
{
    // Пусто
}

