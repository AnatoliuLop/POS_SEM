#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/common.h"
#include "../include/client_api.h"

// CHANGED: храним глобально наш client_id, и индекс змейки
static int g_my_id = -1;
static int g_my_snake_index = -1;

void *client_input_thread(void *arg) {
    GameState *game = (GameState*) arg;
    enable_nonblocking_input();

    // Текущее направление
    char last_dir = 'd'; // по умолчанию

    while (1) {
        char c = getchar();
        if (c == EOF) {
            usleep(50000);
            continue;
        }
        pthread_mutex_lock(&game_mutex);

        if (!game->server_running || game->game_over) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }
        if (g_my_snake_index<0) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        Snake *s = &game->snakes[g_my_snake_index];
        if (!s->active) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        // Запрет 180°
        if (c=='w' || c=='a' || c=='s' || c=='d') {
            if (!((c=='w' && last_dir=='s') || (c=='s' && last_dir=='w') ||
                  (c=='a' && last_dir=='d') || (c=='d' && last_dir=='a'))) {
                s->direction = c;
                last_dir = c;
            }
        }
        pthread_mutex_unlock(&game_mutex);

        usleep(50000);
    }

    disable_nonblocking_input();
    return NULL;
}

void *client_display_thread(void *arg) {
    GameState *game = (GameState*) arg;
    while (1) {
        pthread_mutex_lock(&game_mutex);

        if (!game->server_running || game->game_over) {
            pthread_mutex_unlock(&game_mutex);
            break;
        }

        // CHANGED: стираем консоль
        printf("\033[H\033[J"); 

        // Выводим время
        int elapsed = get_elapsed_seconds(game->start_time);
        if (game->mode == GAME_MODE_TIMED) {
            int left = game->time_limit - elapsed;
            if (left<0) left=0;
            printf("Time left: %d\n", left);
        } else {
            printf("Elapsed: %d\n", elapsed);
        }

        // Выводим счёт
        for (int i=0; i<MAX_PLAYERS; i++) {
            Snake *sn = &game->snakes[i];
            if (sn->active) {
                // CHANGED: если i==1, зелёный цвет
                if (i==1) {
                    printf("\033[32m");
                    printf("Snake (player_id=%d) => score=%d\n", sn->player_id, sn->score);
                    printf("\033[0m"); 
                } else {
                    printf("Snake (player_id=%d) => score=%d\n", sn->player_id, sn->score);
                }
            }
        }

        // Отрисовка поля
        // Если мир с препятствиями, по периметру рисуем "_" и "|" 
        // Но мы уже ставим obstacles[0][x]=1 (и т.д.).
        // Давайте вместо '#' сделаем "_" сверху/снизу и "|" слева/справа

        int maxH = game->height;
        int maxW = game->width;

        // С ограничениями:
        if (maxH>30) maxH=30; 
        if (maxW>80) maxW=80; 
        // (Чтобы не огромная простыня в консоли.)

        for (int y=0; y<maxH; y++) {
            for (int x=0; x<maxW; x++) {
                // препятствие?
                if (game->obstacles[y][x] == 1) {
                    // Если это верхняя/нижняя грань => '_', иначе если левая/правая => '|'
                    if (y==0 || y==game->height-1) {
                        putchar('_');
                    } else if (x==0 || x==game->width-1) {
                        putchar('|');
                    } else {
                        putchar('#'); 
                    }
                    continue;
                }

                // Проверка змей
                char ch='.';
                int color_snake=-1; 
                for (int p=0; p<MAX_PLAYERS; p++) {
                    Snake *sn = &game->snakes[p];
                    if (!sn->active) continue;
                    for (int k=0; k<sn->length; k++) {
                        if (sn->body[k].x==x && sn->body[k].y==y) {
                            if (k==0) ch='O'; else ch='o';
                            color_snake=p; // 0 или 1
                            goto after_find;
                        }
                    }
                }
after_find:;
                // Фрукт?
                if (ch=='.') {
                    for (int f=0; f<MAX_PLAYERS; f++) {
                        if (game->fruits[f].x==x && game->fruits[f].y==y) {
                            ch='F';
                            break;
                        }
                    }
                }
                if (color_snake==1) {
                    // вторая змея (зелёная)
                    printf("\033[32m%c\033[0m", ch);
                } else {
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

// CHANGED: теперь run_client_game принимает client_id
void run_client_game(int client_id)
{
    g_my_id = client_id;

    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), 0666);
    if (shmid<0) {
        printf("[CLIENT] No server found.\n");
        return;
    }
    GameState *game = (GameState*) shmat(shmid, NULL, 0);
    if (game==(void*)-1) {
        perror("[CLIENT] shmat");
        return;
    }

    pthread_mutex_lock(&game_mutex);

    // Ищем, есть ли змея с таким player_id (неактивна?)
    int slot=-1;
    for (int i=0; i<MAX_PLAYERS; i++) {
        if (game->snakes[i].player_id == client_id && game->snakes[i].active) {
            // Мы возвращаемся
            slot=i;
            break;
        }
    }
    // Если не нашли, значит создаём новую?
    if (slot<0) {
        // Ищем свободный
        for (int i=0; i<MAX_PLAYERS; i++) {
            if (!game->snakes[i].active) {
                slot=i;
                // Инициализируем
                Snake *s = &game->snakes[i];
                s->player_id = client_id;
                s->player_connected = 1;
                s->active = 1;
                s->length=3;
                s->direction='d';
                s->score=0;
                // Пример координат
                if (i==0) {
                    s->body[0].x=2;  s->body[0].y=2;
                    s->body[1].x=1;  s->body[1].y=2;
                    s->body[2].x=0;  s->body[2].y=2;
                } else {
                    s->body[0].x=game->width-3;
                    s->body[0].y=game->height-3;
                    s->body[1].x=game->width-4;
                    s->body[1].y=game->height-3;
                    s->body[2].x=game->width-5;
                    s->body[2].y=game->height-3;
                }
                game->connected_players++;
                break;
            }
        }
    } else {
        // slot>=0, змея уже есть, просто player_connected=1
        game->snakes[slot].player_connected=1;
    }
    g_my_snake_index = slot;

    // Если не удалось
    if (g_my_snake_index<0) {
        printf("[CLIENT] No free slot!\n");
        pthread_mutex_unlock(&game_mutex);
        shmdt(game);
        return;
    }

    pthread_mutex_unlock(&game_mutex);

    // Запускаем потоки
    pthread_t thr_in, thr_out;
    pthread_create(&thr_in, NULL, client_input_thread, (void*)game);
    pthread_create(&thr_out, NULL, client_display_thread, (void*)game);

    // Ждем окончания thr_in
    pthread_join(thr_in, NULL);
    pthread_cancel(thr_out);
    pthread_join(thr_out, NULL);

    pthread_mutex_lock(&game_mutex);
    // Если мы закрываем клиента, отметим player_connected=0
    // и обнулим direction, чтобы змейка стояла
    if (g_my_snake_index>=0) {
        Snake *s = &game->snakes[g_my_snake_index];
        if (s->active) {
            s->player_connected=0;
            s->direction=0; // пусть стоит
        }
    }
    pthread_mutex_unlock(&game_mutex);

    shmdt(game);
}

