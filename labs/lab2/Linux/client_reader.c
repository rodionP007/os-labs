// client_reader.c


#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <unistd.h>

#define FILENAME   "/tmp/mmap_lab2_2.dat"
#define FILESIZE   4096

#define FIFO_READY "/tmp/mmap_lab2_2_ready.fifo"
#define FIFO_READ  "/tmp/mmap_lab2_2_read.fifo"

static int   g_fd_file = -1;
static void* g_ptr = NULL;

static int   g_fd_ready_r = -1; // читаем "готово"
static int   g_fd_read_w  = -1; // пишем "прочитано"

static void clearScreen(void) { system("clear"); }

static void pauseEnter(void) {
    printf("\n[Enter] назад в меню...");
    fflush(stdout);
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {}
}

static void header(const char* title) {
    clearScreen();
    printf("========================================\n");
    printf("%s\n", title);
    printf("========================================\n\n");
}

static void perr(const char* where) {
    fprintf(stderr, "[ERROR] %s: %s\n", where, strerror(errno));
}

static void cleanup_all(void) {
    if (g_ptr) { munmap(g_ptr, FILESIZE); g_ptr = NULL; }
    if (g_fd_file != -1) { close(g_fd_file); g_fd_file = -1; }

    if (g_fd_ready_r != -1) { close(g_fd_ready_r); g_fd_ready_r = -1; }
    if (g_fd_read_w  != -1) { close(g_fd_read_w);  g_fd_read_w  = -1; }
}

static int wait_readable_select(int fd, int seconds) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);

    struct timeval tv;
    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    int r = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (r < 0) return -1;
    if (r == 0) return 0;
    return FD_ISSET(fd, &rfds) ? 1 : 0;
}

static void menu_map(void) {
    header("Клиент: выполнить проецирование (open + mmap + FIFO)");

    g_fd_ready_r = open(FIFO_READY, O_RDONLY | O_NONBLOCK);
    if (g_fd_ready_r == -1) {
        perr("open FIFO_READY (read) (сервер должен создать сначала)");
        pauseEnter();
        return;
    }

    g_fd_read_w = open(FIFO_READ, O_WRONLY | O_NONBLOCK);
    if (g_fd_read_w == -1) {
        if (errno == ENXIO) {
            printf("Сервер ещё не открыл FIFO_READ на чтение. Попробуй сначала на сервере пункт 1.\n");
        } else {
            perr("open FIFO_READ (write)");
        }
        pauseEnter();
        return;
    }

    // открыть файл
    g_fd_file = open(FILENAME, O_RDWR, 0666);
    if (g_fd_file == -1) {
        perr("open file (сервер должен создать сначала)");
        pauseEnter();
        return;
    }

    g_ptr = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd_file, 0);
    if (g_ptr == MAP_FAILED) { g_ptr = NULL; perr("mmap"); pauseEnter(); return; }

    printf("OK\nФайл: %s (size=%d)\nАдрес mmap: %p\n", FILENAME, FILESIZE, g_ptr);
    printf("FIFO_READY: %s\nFIFO_READ : %s\n", FIFO_READY, FIFO_READ);
    printf("\nТеперь можно читать (пункт 2).\n");
    pauseEnter();
}

static void menu_read(void) {
    header("Клиент: ожидание данных (select) и чтение из mmap");

    if (!g_ptr) {
        printf("Сначала выполните проецирование (пункт 1).\n");
        pauseEnter();
        return;
    }

    printf("Жду сигнал от сервера (select на FIFO_READY)...\n");

    int wr = wait_readable_select(g_fd_ready_r, 60);
    if (wr < 0) { perr("select"); pauseEnter(); return; }
    if (wr == 0) {
        printf("Timeout: сервер не прислал данные за 60 секунд.\n");
        pauseEnter();
        return;
    }

    char sig;
    ssize_t rd = read(g_fd_ready_r, &sig, 1);
    if (rd <= 0) {
        perr("read FIFO_READY (signal)");
        pauseEnter();
        return;
    }

    printf("\nClient received: %s\n", (char*)g_ptr);

    // подтвердить чтение серверу
    const char ack = '1';
    if (write(g_fd_read_w, &ack, 1) != 1) {
        perr("write FIFO_READ (ack)");
        pauseEnter();
        return;
    }

    printf("\nПодтверждение чтения отправлено.\n");
    printf("Сервер после этого снимет mmap и удалит файл.\n");
    pauseEnter();
}

static void print_menu(void) {
    printf("============== CLIENT ==============\n");
    printf("mmap: %s\n", g_ptr ? "есть" : "нет");
    printf("------------------------------------\n");
    printf("1) выполнить проецирование\n");
    printf("2) прочитать данные\n");
    printf("3) завершить работу\n");
    printf("====================================\n");
}

int main(void) {
    while (1) {
        clearScreen();
        print_menu();
        printf("Выберите пункт: ");
        fflush(stdout);

        char line[32];
        if (!fgets(line, sizeof(line), stdin)) break;
        int c = atoi(line);

        if (c == 1) menu_map();
        else if (c == 2) menu_read();
        else if (c == 3) {
            header("Клиент: завершение работы");
            cleanup_all();
            printf("Выход.\n");
            break;
        } else {
            printf("Неверный пункт.\n");
            pauseEnter();
        }
    }
    return 0;
}
