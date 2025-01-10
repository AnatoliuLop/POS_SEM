#ifndef COMMON_H
#define COMMON_H

#define GRID_SIZE 10
#define MAX_PLAYERS 2
#define SHARED_MEMORY_KEY 5678

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
    int grid[GRID_SIZE][GRID_SIZE]; // Игровое поле
    PlayerData players[MAX_PLAYERS]; // Данные о каждом игроке
    int fruit_x;
    int fruit_y;
    int game_over;        // Флаг окончания игры
    int player_count;     // Количество игроков
    int ready_players;    // Сколько игроков готовы
} GameState;

#endif

