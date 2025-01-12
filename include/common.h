#ifndef COMMON_H
#define COMMON_H
#include <pthread.h>

#define GRID_SIZE 25
#define MAX_PLAYERS 2
#define SHARED_MEMORY_KEY 2348


typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    Position snake[100]; // Тело змейки
    int length;          // Длина змейки
    int score;           // Очки игрока
    int active;          // Активен ли игрок
    char direction;      // Направление движения
} PlayerData;

typedef struct {
    int grid[GRID_SIZE][GRID_SIZE];     // Игровое поле
    PlayerData players[MAX_PLAYERS];   // Данные о каждом игроке
    int fruit_x;                       // Координаты фрукта
    int fruit_y;                       // Координаты фрукта
    int game_over;                     // Флаг окончания игры
    int player_count;                  // Количество игроков
    int ready_players;                 // Сколько игроков готово
} GameState;

extern pthread_mutex_t game_mutex; // Объявление мьютекса
// Прототипы функций для управления неблокирующим вводом
void enable_nonblocking_input();
void disable_nonblocking_input();

#endif // COMMON_H

