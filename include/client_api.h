#ifndef CLIENT_API_H
#define CLIENT_API_H

#include "common.h"

// Функции для клиента
void *client_input_thread(void *arg);
void *client_display_thread(void *arg);
void run_client_game(int client_id); 
// CHANGED: теперь run_client_game принимает некий client_id, 
// чтобы можно было «вернуться» к своей змее

#endif

