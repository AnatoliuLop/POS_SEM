#ifndef SERVER_API_H
#define SERVER_API_H

#include "common.h"

void server_init_game(GameState *game, int width, int height,
                      GameMode mode, int time_limit, WorldType wtype);

void *server_game_loop_thread(void *arg);

void server_move_snake(GameState *game, int i);
void server_check_collisions(GameState *game, int i);
void server_generate_fruits(GameState *game);

void server_cleanup_and_exit();
void server_stop(int signo);

#endif

