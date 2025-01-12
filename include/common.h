#ifndef COMMON_H
#define COMMON_H

#include <time.h>
#include <pthread.h>

#define SHARED_MEMORY_KEY 2348
#define MAX_PLAYERS 2
#define MAX_SNAKE_LENGTH 200

// Maximálne rozmery sveta
#define MAX_WIDTH  100
#define MAX_HEIGHT 100

// Herné režimy
typedef enum {
    GAME_MODE_STANDARD = 0,
    GAME_MODE_TIMED
} GameMode;

// Typ sveta
typedef enum {
    WORLD_NO_OBSTACLES = 0,
    WORLD_WITH_OBSTACLES
} WorldType;

// Jednoduchá štruktúra pre pozíciu (x, y)
typedef struct {
    int x;
    int y;
} Position;

// Dáta o jednom hadíkovi
typedef struct {
    int player_id;         // ID hráča
    int player_connected;  // 1=hráč je pripojený, 0=odpojený (hadík stojí)
    int paused;            // 1=hadík je pozastavený
    time_t pause_end_time; // kedy sa má opäť pohnúť po pauze (teraz+3s)

    Position body[MAX_SNAKE_LENGTH];
    int length;
    int active;       // 1=živý, 0=umrel
    char direction;   // 'w','a','s','d' alebo 0 (stojí)
    int score;
} Snake;

// Spoločný stav hry v zdieľanej pamäti
typedef struct {
    int width;
    int height;
    GameMode mode;  
    WorldType wtype;
    int time_limit;    
    time_t start_time;

    // Hráči/hadíci
    Snake snakes[MAX_PLAYERS];
    int connected_players;   
    int game_over;
    int server_running;

    // Ovocie (€). Toľko kúskov, koľko aktívnych hadíkov
    Position fruits[MAX_PLAYERS];

    // V režime STANDARD: čas, kedy odišiel posledný hráč
    time_t last_snake_left_time;

    // Prekážky (1=prekážka, 0=voľno)
    int obstacles[MAX_HEIGHT][MAX_WIDTH];
} GameState;

// Globálny mutex
extern pthread_mutex_t game_mutex;

// Funkcie pre neblokujúci vstup
void enable_nonblocking_input();
void disable_nonblocking_input();

// Vráti, koľko sekúnd uplynulo od start
int get_elapsed_seconds(time_t start);

#endif

