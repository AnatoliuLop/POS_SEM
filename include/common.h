#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <pthread.h>

#define SHARED_MEMORY_KEY 2348
#define MAX_PLAYERS 2
#define MAX_SNAKE_LENGTH 100

// CHANGED: ограничиваем поле 100x100
#define MAX_WIDTH  100
#define MAX_HEIGHT 100

typedef enum {
    GAME_MODE_STANDARD = 0,
    GAME_MODE_TIMED
} GameMode;

typedef enum {
    WORLD_NO_OBSTACLES = 0,
    WORLD_WITH_OBSTACLES
} WorldType;

// Позиция (x, y)
typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    int player_id;        // CHANGED: у каждой змейки - id хозяина (клиента)
    int player_connected; // CHANGED: 1=этот игрок сейчас в клиенте, 0=вышел, но змея остаётся

    Position body[MAX_SNAKE_LENGTH];
    int length;
    int active;     // 1=змея «живая», 0=мертва
    char direction; // 'w','a','s','d'
    int score;
} Snake;

typedef struct {
    int width;
    int height;
    GameMode mode;
    WorldType wtype;
    int time_limit;
    time_t start_time;

    // Змейки
    Snake snakes[MAX_PLAYERS];
    int connected_players;   // число "подключённых" (player_connected=1)
    int game_over;
    int server_running;

    // Фрукты (до 2 шт.)
    Position fruits[MAX_PLAYERS];

    // Когда последний игрок ушёл (для Standard)
    time_t last_snake_left_time;

    // Препятствия
    int obstacles[MAX_HEIGHT][MAX_WIDTH]; 
} GameState;

// Глобальный мьютекс
extern pthread_mutex_t game_mutex;

// Неблокирующий ввод
void enable_nonblocking_input();
void disable_nonblocking_input();

// Вспомогательные
int get_elapsed_seconds(time_t start);

#endif

