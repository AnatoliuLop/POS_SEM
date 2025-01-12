#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include "../include/common.h"

// Статические переменные для хранения настроек терминала
static struct termios oldt, newt;

pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER; // Инициализация мьютекса


// Включение неблокирующего ввода
void enable_nonblocking_input() {
    // Сохраняем текущие настройки терминала
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    // Отключаем канонический режим и эхо-вывод
    newt.c_lflag &= ~(ICANON | ECHO);

    // Применяем новые настройки
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

// Отключение неблокирующего ввода
void disable_nonblocking_input() {
    // Восстанавливаем старые настройки терминала
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
}
