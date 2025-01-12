#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "../include/common.h"

pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;

static struct termios oldt, newt;

void enable_nonblocking_input() {
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void disable_nonblocking_input() {
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}

int get_elapsed_seconds(time_t start) {
    return (int)(time(NULL) - start);
}

