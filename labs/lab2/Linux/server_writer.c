// server_writer.c

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

static int   g_fd_ready_w = -1; // пишем "готово"
static int   g_fd_read_r  = -1; // читаем "прочитано"

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

static void cleanup_mapping_keep_files(void) {
    if (g_ptr) { munmap(g_ptr, FILESIZE); g_ptr = NULL; }
    if (g_fd_file != -1) { close(g_fd_file); g_fd_file = -1; }

    if (g_fd_ready_w != -1) { close(g_fd_ready_w); g_fd_ready_w = -1; }
    if (g_fd_read_r  != -1) { close(g_fd_read_r);  g_fd_read_r  = -1; }
}

static void cleanup_all_and_delete(void) {
    cleanup_mapping_keep_files();
    unlink(FILENAME);
    unlink(FIFO_READY);
    unlink(FIFO_READ);
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
    if (r == 0) return 0; // timeout
    return FD_ISSET(fd, &rfds) ? 1 : 0;
}

static void menu_map(void) {
    header("Сервер: выполнить проецирование (создать файл + mmap + FIFO)");

    // очистим старые артефакты (если остались)
    unlink(FILENAME);
    unlink(FIFO_READY);
    unlink(FIFO_READ);

    // создаём FIFO для синхронизации
    if (mkfifo(FIFO_READY, 0666) != 0) { perr("mkfifo READY"); pauseEnter(); return; }
    if (mkfifo(FIFO_READ,  0666) != 0) { perr("mkfifo READ");  pauseEnter(); return; }

    // создаём файл
    g_fd_file = open(FILENAME, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if (g_fd_file == -1) { perr("open file"); pauseEnter(); return; }

    if (ftruncate(g_fd_file, FILESIZE) != 0) { perr("ftruncate"); pauseEnter(); return; }

    // mmap
    g_ptr = mmap(NULL, FILESIZE, PROT_READ | PROT_WRITE, MAP_SHARED, g_fd_file, 0);
    if (g_ptr == MAP_FAILED) { g_ptr = NULL; perr("mmap"); pauseEnter(); return; }
    memset(g_ptr, 0, FILESIZE);

    // Открываем FIFO:
    // - READY: мы будем писать, но open(O_WRONLY) блокирует пока клиент не откроет на чтение.
    //   Чтобы сервер не зависал, откроем в O_WRONLY|O_NONBLOCK, а если нет читателя — скажем запустить клиента.
    g_fd_ready_w = open(FIFO_READY, O_WRONLY | O_NONBLOCK);
    if (g_fd_ready_w == -1) {
        if (errno == ENXIO) {
            printf("FIFO_READY пока без читателя. Запусти клиент и сделай у него 'выполнить проецирование',\n");
            printf("потом вернись и снова выбери пункт 1 (или просто пункт 2 — после открытия FIFO).\n");
        } else {
            perr("open FIFO_READY (write)");
        }
        // не выходим: mmap уже есть; можно позже переоткрыть FIFO при записи
    }

    // - READ: мы будем читать подтверждение; open(O_RDONLY|O_NONBLOCK) не блокирует
    g_fd_read_r = open(FIFO_READ, O_RDONLY | O_NONBLOCK);
    if (g_fd_read_r == -1) { perr("open FIFO_READ (read)"); pauseEnter(); return; }

    printf("OK\nФайл: %s (size=%d)\nАдрес mmap: %p\n", FILENAME, FILESIZE, g_ptr);
    printf("FIFO_READY: %s\nFIFO_READ : %s\n", FIFO_READY, FIFO_READ);
    printf("\nДальше: клиент -> 'выполнить проецирование', затем сервер -> 'записать данные'.\n");
    pauseEnter();
}

static void ensure_ready_fifo_open(void) {
    if (g_fd_ready_w != -1) return;
    g_fd_ready_w = open(FIFO_READY, O_WRONLY | O_NONBLOCK);
}

static void menu_write(void) {
    header("Сервер: записать данные и ждать чтения (select)");

    if (!g_ptr) {
        printf("Сначала выполните проецирование (пункт 1).\n");
        pauseEnter();
        return;
    }

    ensure_ready_fifo_open();
    if (g_fd_ready_w == -1) {
        if (errno == ENXIO) {
            printf("Клиент ещё не открыл FIFO_READY на чтение.\n");
            printf("Запусти клиент, выбери 'выполнить проецирование', затем попробуй снова.\n");
        } else {
            perr("open FIFO_READY (write)");
        }
        pauseEnter();
        return;
    }

    char buf[FILESIZE];
    printf("Введите строку (до %d байт):\n> ", FILESIZE - 1);
    fflush(stdout);

    if (!fgets(buf, sizeof(buf), stdin)) {
        printf("Ввод отменён.\n");
        pauseEnter();
        return;
    }
    buf[strcspn(buf, "\n")] = 0;

    memset(g_ptr, 0, FILESIZE);
    strncpy((char*)g_ptr, buf, FILESIZE - 1);

    // Сигнал "готово" (1 байт)
    const char one = '1';
    if (write(g_fd_ready_w, &one, 1) != 1) {
        perr("write FIFO_READY");
        pauseEnter();
        return;
    }

    printf("\nДанные записаны. Жду подтверждения чтения от клиента (select на FIFO_READ)...\n");

    // Ждём readable на FIFO_READ
    int wr = wait_readable_select(g_fd_read_r, 60);
    if (wr < 0) { perr("select"); pauseEnter(); return; }
    if (wr == 0) {
        printf("Timeout: клиент не подтвердил чтение за 60 секунд.\n");
        pauseEnter();
        return;
    }

    char ack;
    ssize_t rd = read(g_fd_read_r, &ack, 1);
    if (rd <= 0) {
        perr("read FIFO_READ (ack)");
        pauseEnter();
        return;
    }

    printf("Клиент подтвердил чтение.\n");
    printf("\nСнимаю отображение и удаляю файл/каналы...\n");
    cleanup_all_and_delete();
    printf("Готово. Чтобы начать заново — снова пункт 1.\n");
    pauseEnter();
}

static void print_menu(void) {
    printf("============== SERVER ==============\n");
    printf("mmap: %s\n", g_ptr ? "есть" : "нет");
    printf("------------------------------------\n");
    printf("1) выполнить проецирование\n");
    printf("2) записать данные\n");
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
        else if (c == 2) menu_write();
        else if (c == 3) {
            header("Сервер: завершение работы");
            cleanup_all_and_delete();
            printf("Выход.\n");
            break;
        } else {
            printf("Неверный пункт.\n");
            pauseEnter();
        }
    }
    return 0;
}
