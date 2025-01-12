#ifndef CLIENT_API_H
#define CLIENT_API_H

#include "common.h"

// Spustenie hlavného menu klienta
void run_client_menu();

// Funkcia na pripojenie sa k hre (definovaná v client_game.c)
void join_game(int client_id);

#endif

