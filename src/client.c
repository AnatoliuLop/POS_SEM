#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/shm.h>
#include "../include/common.h"

// Поток для обработки пользовательского ввода (W, A, S, D)
void *handle_input(void *arg) {
    GameState *game = (GameState *)arg;

    // Узнаём, какой у нас индекс игрока
    // (game->ready_players-1) мы выставили при подключении
    int player_id;
    pthread_mutex_lock(&game_mutex);
    player_id = game->ready_players - 1;
    pthread_mutex_unlock(&game_mutex);

    while (!game->game_over) {
        char input = getchar();

        // Если введена одна из W, A, S, D
        if (input == 'w' || input == 'a' || input == 's' || input == 'd') {
            pthread_mutex_lock(&game_mutex);
            game->players[player_id].direction = input;
            pthread_mutex_unlock(&game_mutex);

            printf("[CLIENT] Sent to server: Player %d pressed %c\n", 
                    player_id + 1, input);
        }
        usleep(50000); // небольшая задержка
    }

    return NULL;
}

// Поток отображения игрового поля
void *handle_display(void *arg) {
    GameState *game = (GameState *)arg;

    // Узнаём свой индекс (как выше)
    int player_id;
    pthread_mutex_lock(&game_mutex);
    player_id = game->ready_players - 1;
    pthread_mutex_unlock(&game_mutex);

    while (!game->game_over) {
        pthread_mutex_lock(&game_mutex);

        // Если ещё не все подключились:
        if (game->ready_players < game->player_count) {
            // Можно вывести более конкретное сообщение,
            // если выбрано 2 игрока, а подключен 1
            if (game->player_count == 2 && game->ready_players == 1) {
                printf("Waiting for the second player...\n");
            } else {
                // Если вдруг логика изменится на 3-4 игроков
                printf("Waiting for other players to connect...\n");
            }
            pthread_mutex_unlock(&game_mutex);
            usleep(500000);
            continue;
        }

        // Если сервер остановлен (например, нажали p)
        if (!game->server_running) {
            printf("SERVER STOPPED THE GAME!\n");
            pthread_mutex_unlock(&game_mutex);
            exit(0);
        }

        // Отрисовываем поле (минимально)
        // system("clear"); // Если хотите очищать экран
        for (int y = 0; y < GRID_SIZE; y++) {
            for (int x = 0; x < GRID_SIZE; x++) {
                int val = game->grid[y][x];
                if (val == 0) {
                    printf(".");
                } else if (val == 1) {
                    printf("O"); // Первый игрок
                } else if (val == 2) {
                    printf("G"); // Второй игрок
                } else if (val == 3) {
                    printf("F"); // Фрукт
                }
            }
            printf("\n");
        }
        // Выводим очки
        printf("Your score: %d (Player %d)\n", 
                game->players[player_id].score, player_id+1);

        pthread_mutex_unlock(&game_mutex);

        // Небольшая задержка
        usleep(200000);
    }

    return NULL;
}

int main() {
    printf("Trying to connect to shared memory...\n");

    // Пытаемся получить доступ к разделяемой памяти
    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    printf("Connected to shared memory. SHMID: %d\n", shmid);

    // Присоединяемся
    GameState *game = (GameState *)shmat(shmid, NULL, 0);
    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }

    // Проверяем, есть ли ещё «место» для нас
    pthread_mutex_lock(&game_mutex);
    if (game->ready_players >= game->player_count) {
        printf("Error: Game is already full!\n");
        pthread_mutex_unlock(&game_mutex);
        shmdt((void *)game);
        exit(1);
    }
    // Увеличиваем счётчик подключившихся
    game->ready_players++;
    // Наш ID = (ready_players - 1)
    int player_id = game->ready_players - 1;
    pthread_mutex_unlock(&game_mutex);

    printf("Client started. You are Player %d!\n", player_id+1);

    // Включаем неблокирующий ввод
    enable_nonblocking_input();

    // Создаём потоки для чтения ввода и отображения
    pthread_t input_thr, display_thr;
    pthread_create(&input_thr, NULL, handle_input, (void *)game);
    pthread_create(&display_thr, NULL, handle_display, (void *)game);

    // Ждём завершения
    pthread_join(input_thr, NULL);
    pthread_join(display_thr, NULL);

    // Выключаем неблокирующий ввод
    disable_nonblocking_input();

    // Отключаемся от памяти
    shmdt((void *)game);

    return 0;
}

