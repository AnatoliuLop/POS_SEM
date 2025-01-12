#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/common.h"

// Функция для генерации фрукта на пустом месте
void generate_fruit(GameState *game) {
    int fruit_placed = 0;
    while (!fruit_placed) {
        int x = rand() % GRID_SIZE;
        int y = rand() % GRID_SIZE;
        if (game->grid[y][x] == 0) {
            game->grid[y][x] = 3; // 3 обозначаем фрукт
            game->fruit_x = x;
            game->fruit_y = y;
            fruit_placed = 1;
        }
    }
}

// Инициализация игры (поле, позиции змей, счёт и т.п.)
void initialize_game(GameState *game) {
    pthread_mutex_lock(&game_mutex);

    // Обнуление поля
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            game->grid[y][x] = 0;
        }
    }

    // Устанавливаем начальную информацию по игрокам
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (i < game->player_count) {
            // Этот игрок участвует
            game->players[i].length = 1;
            game->players[i].score = 0;
            game->players[i].direction = 'd'; // Пусть по умолчанию смотрит вправо
            game->players[i].active = 1;
            // Пример расстановки: первый игрок (i=0) в (1,1), второй - в (GRID_SIZE-2, GRID_SIZE-2)
            if (i == 0) {
                game->players[i].snake[0].x = 1;
                game->players[i].snake[0].y = 1;
            } else {
                game->players[i].snake[0].x = GRID_SIZE - 2;
                game->players[i].snake[0].y = GRID_SIZE - 2;
            }
            // Ставим на поле (1 обозначим первого игрока, 2 - второго)
            game->grid[ game->players[i].snake[0].y ][ game->players[i].snake[0].x ] = i+1;
        } else {
            // Если, например, выбрано 1 игрок, второй игрок не активен
            game->players[i].active = 0;
            game->players[i].length = 0;
        }
    }

    // Генерируем фрукт
    generate_fruit(game);

    // Остальные флаги
    game->ready_players = 0;   // Сколько уже подключилось
    game->game_over = 0;
    game->server_running = 1;  // Сервер активен

    pthread_mutex_unlock(&game_mutex);
}

// Основной игровой цикл (в отдельном потоке)
void *game_loop(void *arg) {
    GameState *game = (GameState *)arg;

    printf("[GAME_LOOP] Started! Waiting for moves...\n");

    // Пока сервер не остановлен и игра не завершена
    while (game->server_running && !game->game_over) {
        pthread_mutex_lock(&game_mutex);

        // Обработка движения для каждого активного игрока
        for (int i = 0; i < game->player_count; i++) {
            PlayerData *player = &game->players[i];
            if (player->active) {
                // Если был введён direction (w, a, s, d)
                if (player->direction != 0) {
                    Position head = player->snake[0];
                    Position new_head = head;

                    // Смотрим, куда сдвинуть голову
                    switch (player->direction) {
                        case 'w': new_head.y--; break;
                        case 's': new_head.y++; break;
                        case 'a': new_head.x--; break;
                        case 'd': new_head.x++; break;
                    }

                    // Убираем старую голову с поля (упрощённо)
                    // Но для полноценной змейки нужно освобождать хвост.
                    // Здесь минимальная логика для демонстрации
                    game->grid[ head.y ][ head.x ] = 0;

                    // Перемещаем всё тело змейки (сдвиг)
                    // (в этой логике длина 1, но можно расширить)
                    for (int j = player->length - 1; j > 0; j--) {
                        player->snake[j] = player->snake[j - 1];
                    }
                    player->snake[0] = new_head;
                    game->grid[new_head.y][new_head.x] = i + 1;

                    // Сбрасываем направление после обработки
                    player->direction = 0;
                }
            }
        }

        pthread_mutex_unlock(&game_mutex);
        usleep(200000); // 0.2 секунды «тик» игрового цикла
    }

    printf("[GAME_LOOP] Exiting. game_over=%d, server_running=%d\n", 
           game->game_over, game->server_running);
    return NULL;
}

// Завершение: убираем сегмент, останавливаем сервер
void cleanup_and_exit(int shmid, GameState *game) {
    printf("Cleaning up and exiting...\n");

    pthread_mutex_lock(&game_mutex);
    game->server_running = 0; // Останавливаем сервер
    pthread_mutex_unlock(&game_mutex);

    if (game != (GameState *)-1) {
        shmdt((void *)game);
    }
    if (shmid >= 0) {
        shmctl(shmid, IPC_RMID, NULL);
    }
    exit(0);
}

// Поток для обработки клавиши 'p' (чтобы завершить сервер)
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
    // Создаём/получаем общий сегмент памяти
    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    // Подключаемся к общему сегменту
    GameState *game = (GameState *)shmat(shmid, NULL, 0);
    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }
    printf("Shared memory created. SHMID: %d\n", shmid);

    // Считываем, сколько игроков будет
    printf("Server started...\n");
    printf("Select number of players (1 or 2): ");
    scanf("%d", &game->player_count);
    // Обязательно делаем проверку
    if (game->player_count < 1 || game->player_count > 2) {
        printf("Invalid number of players! Must be 1 or 2.\n");
        // Отключаемся и удаляем
        shmdt((void *)game);
        shmctl(shmid, IPC_RMID, NULL);
        return 1;
    }

    // Инициализируем игру (сразу расставляем змейки, фрукт и т.п.)
    initialize_game(game);

    printf("Waiting for players to connect...\n");

    // Ждём, пока подключатся все нужные игроки
    while (1) {
        pthread_mutex_lock(&game_mutex);
        int rp = game->ready_players;
        int pc = game->player_count;
        pthread_mutex_unlock(&game_mutex);

        if (rp >= pc) {
            break; // Все игроки подключились
        }
        printf("Players connected: %d/%d\n", rp, pc);
        usleep(500000);
    }

    printf("All players connected! Starting the game...\n");

    // Создаём потоки
    pthread_t game_thread, input_thread_handle;
    pthread_create(&game_thread, NULL, game_loop, (void *)game);
    pthread_create(&input_thread_handle, NULL, input_thread, (void *)&shmid);

    // Дожидаемся завершения
    pthread_join(game_thread, NULL);
    pthread_join(input_thread_handle, NULL);

    // Если когда-то выйдем из game_loop, то завершаем
    cleanup_and_exit(shmid, game);

    return 0;
}

