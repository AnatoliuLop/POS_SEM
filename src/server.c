#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "../include/common.h"

// Функция для инициализации игрового поля
void generate_fruit(GameState *game);
void initialize_game(GameState *game, int player_count) {
    memset(game->grid, 0, sizeof(game->grid)); // Очистка поля

    // Настройка общего состояния игры
    game->game_over = 0;
    game->player_count = player_count;
    game->ready_players = 0;

    // Инициализация игроков
    for (int i = 0; i < MAX_PLAYERS; i++) {
        game->players[i].active = 0;
        game->players[i].score = 0;
        game->players[i].length = 1; // Начальная длина змейки
        game->players[i].snake[0].x = GRID_SIZE / 2 + i; // Разные начальные позиции
        game->players[i].snake[0].y = GRID_SIZE / 2;
        game->players[i].direction = 'd'; // Начальное движение вправо
    }

    // Генерация первого фрукта
    generate_fruit(game);
}

// Функция для генерации фрукта вне змейки
void generate_fruit(GameState *game) {
    int valid_position = 0;

    while (!valid_position) {
        // Генерируем случайные координаты
        int new_x = rand() % GRID_SIZE;
        int new_y = rand() % GRID_SIZE;

        valid_position = 1; // Считаем позицию валидной, пока не докажем обратное

        // Проверяем, не находится ли фрукт внутри какой-либо змейки
        for (int i = 0; i < game->player_count; i++) {
            for (int j = 0; j < game->players[i].length; j++) {
                if (game->players[i].snake[j].x == new_x && game->players[i].snake[j].y == new_y) {
                    valid_position = 0; // Позиция занята, продолжаем цикл
                    break;
                }
            }
            if (!valid_position) break;
        }

        if (valid_position) {
            game->fruit_x = new_x;
            game->fruit_y = new_y;
            game->grid[new_y][new_x] = 2; // Обозначаем фрукт на поле
        }
    }
}

// Проверка столкновения змейки с телом или другими игроками
int check_collision(GameState *game, int player_id) {
    Position head = game->players[player_id].snake[0];

    // Проверка на столкновение с телом своей змейки
    for (int i = 1; i < game->players[player_id].length; i++) {
        if (head.x == game->players[player_id].snake[i].x && head.y == game->players[player_id].snake[i].y) {
            return 1;
        }
    }

    // Проверка на столкновение с другими змейками
    for (int i = 0; i < game->player_count; i++) {
        if (i != player_id) {
            for (int j = 0; j < game->players[i].length; j++) {
                if (head.x == game->players[i].snake[j].x && head.y == game->players[i].snake[j].y) {
                    return 1;
                }
            }
        }
    }
    return 0;
}

// Функция для обновления состояния игры
void update_game(GameState *game) {
    for (int i = 0; i < game->player_count; i++) {
        if (!game->players[i].active) continue;

        Position new_head = game->players[i].snake[0];

        // Движение головы змейки
        if (game->players[i].direction == 'w') new_head.y -= 1;
        else if (game->players[i].direction == 's') new_head.y += 1;
        else if (game->players[i].direction == 'a') new_head.x -= 1;
        else if (game->players[i].direction == 'd') new_head.x += 1;

        // Проверка выхода за границы
        if (new_head.x < 0) new_head.x = GRID_SIZE - 1;
        if (new_head.x >= GRID_SIZE) new_head.x = 0;
        if (new_head.y < 0) new_head.y = GRID_SIZE - 1;
        if (new_head.y >= GRID_SIZE) new_head.y = 0;

        // Проверка на столкновение
        if (check_collision(game, i)) {
            printf("Player %d lost! Final score: %d\n", i, game->players[i].score);
            game->game_over = 1;
            return;
        }

        // Проверка на поедание фрукта
        int ate_fruit = (new_head.x == game->fruit_x && new_head.y == game->fruit_y);
        if (ate_fruit) {
            game->players[i].score++;
            game->players[i].length++;
            generate_fruit(game); // Генерируем новый фрукт
        }

        // Перемещение змейки
        for (int j = game->players[i].length - 1; j > 0; j--) {
            game->players[i].snake[j] = game->players[i].snake[j - 1];
        }
        game->players[i].snake[0] = new_head;
    }

    // Очистка поля и перерисовка змей
    memset(game->grid, 0, sizeof(game->grid));
    for (int i = 0; i < game->player_count; i++) {
        for (int j = 0; j < game->players[i].length; j++) {
            game->grid[game->players[i].snake[j].y][game->players[i].snake[j].x] = 1;
        }
    }
    game->grid[game->fruit_y][game->fruit_x] = 2;
}

// Функция для отрисовки игрового поля
void draw_game(GameState *game) {
    printf("\033[H\033[J");
    for (int y = 0; y < GRID_SIZE; y++) {
        for (int x = 0; x < GRID_SIZE; x++) {
            if (game->grid[y][x] == 1) printf("O ");
            else if (game->grid[y][x] == 2) printf("F ");
            else printf(". ");
        }
        printf("\n");
    }
    printf("\n");
    for (int i = 0; i < game->player_count; i++) {
        printf("Player %d Score: %d\n", i, game->players[i].score);
    }
}

int main() {
    printf("Server started...\n");

    // Создание сегмента общей памяти
    int shmid = shmget(SHARED_MEMORY_KEY, sizeof(GameState), IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    GameState *game = (GameState *)shmat(shmid, NULL, 0);
    if (game == (GameState *)-1) {
        perror("shmat");
        exit(1);
    }

    // Выбор количества игроков
    int player_count;
    printf("Select number of players (1 or 2): ");
    scanf("%d", &player_count);
    if (player_count < 1 || player_count > MAX_PLAYERS) {
        printf("Invalid number of players. Exiting.\n");
        shmdt((void *)game);
        exit(1);
    }

    initialize_game(game, player_count);
    printf("Waiting for players to connect...\n");

    // Ждём, пока все игроки подключатся
    while (game->ready_players < game->player_count) {
        usleep(100000);
    }

    // Отсчёт перед началом игры
    for (int i = 3; i > 0; i--) {
        printf("%d...\n", i);
        sleep(1);
    }
    printf("Start!\n");

    // Основной цикл игры
    while (!game->game_over) {
        update_game(game);
        draw_game(game);
        usleep(200000);
    }

    printf("Game has ended.\n");
    shmdt((void *)game);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}

