#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "../include/common.h"
#include "../include/client_api.h"

static void menu_print() {
    printf("\n=== HLAVNE MENU ===\n");
    printf("1. Nova hra (spusti novy server)\n");
    printf("2. Pripojit sa k hre\n");
    printf("3. (Pokracovat v hre) - DEMO: zatial nie\n");
    printf("4. Koniec\n");
    printf("Volba: ");
}

// Spusti novy server s parametrami
static void start_new_server() {
    int w,h,mode;
    int tlimit=0;
    int wtype;

    printf("Zadajte sirku (max 100): ");
    scanf("%d",&w);
    printf("Zadajte vysku (max 100): ");
    scanf("%d",&h);
    printf("Rezim? (0=STANDARD, 1=TIMED): ");
    scanf("%d",&mode);
    if (mode==1) {
        printf("Casovy limit (s): ");
        scanf("%d",&tlimit);
    }
    printf("Typ sveta? (0=bez prekazok, 1=s prekazkami): ");
    scanf("%d",&wtype);

    // Konvertujeme do stringov
    char w_str[16], h_str[16], m_str[16], t_str[16], wt_str[16];
    sprintf(w_str, "%d", w);
    sprintf(h_str, "%d", h);
    sprintf(m_str, "%d", mode);
    sprintf(t_str, "%d", tlimit);
    sprintf(wt_str, "%d", wtype);

    // fork + execl server
    pid_t pid = fork();
    if (pid==0) {
        // child = spust server s argumentami
        execl("./server", "./server",
              w_str, h_str, m_str, t_str, wt_str, 
              (char*)NULL);
        perror("execl");
        exit(1);
    } else {
        // rodic = klient
        // pockame 2s aby sa server stihol inicializovat
        sleep(2);
        // a potom sa vratime do menu
    }
}

void run_client_menu() {
    while(1) {
        menu_print();
        int volba;
        scanf("%d",&volba);

        if (volba==1) {
            // Nova hra
            start_new_server();
        }
        else if (volba==2) {
            // Pripojit
            printf("Zadajte svoj player_id (integer): ");
            int pid;
            scanf("%d",&pid);
            join_game(pid);
        }
        else if (volba==3) {
            // zatial demo
            printf("Pokracovat zatial nie je implementovane.\n");
        }
        else if (volba==4) {
            printf("Koniec klienta.\n");
            exit(0);
        }
        else {
            printf("Neplatna volba.\n");
        }
    }
}

int main() {
    printf("=== KLIENT SPUSTENY ===\n");
    run_client_menu();
    return 0;
}

