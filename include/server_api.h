#ifndef SERVER_API_H
#define SERVER_API_H

#include "common.h"

// Inicializácia hry
void server_init_game(GameState *game, int width, int height,
                      GameMode mode, int time_limit, WorldType wtype, int max_players);

// Hlavná herná slučka
void *server_game_loop_thread(void *arg);

// Posun hadíka
void server_move_snake(GameState *game, int i);

// Kontrola kolízií
void server_check_collisions(GameState *game, int i);

// Generovanie ovocia
void server_generate_fruits(GameState *game);

// Čistenie a ukončenie servera
void server_cleanup_and_exit();
void server_stop(int signo);

#endif

