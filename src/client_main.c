#include <stdio.h>
#include <stdlib.h>
#include "../include/common.h"
#include "../include/client_api.h"

int main()
{
    printf("=== CLIENT ===\n");

    // Спросим у пользователя некий client_id
    // Если он тот же, то вернемся к своей змейке
    // Примерно так:
    int my_id;
    printf("Enter your 'client_id' (any integer): ");
    scanf("%d", &my_id);

    run_client_game(my_id);

    printf("[CLIENT] Exiting.\n");
    return 0;
}

