#ifndef COMMON_H
#define COMMON_H

#define SHARED_MEMORY_KEY 1234
#define MAX_PLAYERS 2
#define GRID_SIZE 20

typedef struct {
    int x;
    int y;
} Position;

typedef struct {
    Position snake[100];
    int length;
    int score;
} PlayerData;

#endif

