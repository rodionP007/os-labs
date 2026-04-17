// Reader.cpp
// cl /EHsc /std:c++17 Reader.cpp winmm.lib

#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <random>
#include <sstream>

#pragma comment(lib, "winmm.lib")

static const int PAGES_COUNT = 17; // TODO: заменить на сумму цифр студбилета без первой цифры
static const wchar_t* MAPPING_NAME = L"Local\\LR4_RW_FILE_MAPPING";
static const wchar_t* INIT_MUTEX_NAME = L"Local\\LR4_RW_INIT_MUTEX";

static std::wstring RcMutexName(int page) {
    wchar_t buf[64];
    wsprintfW(buf, L"Local\\LR4_RC_MUTEX_%d", page);
    return std::wstring(buf);
}

static std::wstring WrtSemaphoreName(int page) {
    wchar_t buf[64];
    wsprintfW(buf, L"Local\\LR4_WRT_SEM_%d", page);
    return std::wstring(buf);
}

struct SharedHeader {
    DWORD magic;
    DWORD pageSize;
    DWORD pagesCount;
    volatile LONG readerCounts[256];
};

struct SharedContext {
    HANDLE hMap = NULL;
    HANDLE hInitMutex = NULL;
    LPBYTE base = nullptr;
    SharedHeader* hdr = nullptr;
    char* pages = nullptr;
    DWORD totalSize = 0;
    DWORD pageSize = 0;
};

static std::mt19937& rng() {
    static std::mt19937 gen(GetTickCount() ^ GetCurrentProcessId());
    return gen;
}

static int rndInt(int a, int b) {
    std::uniform_int_distribution<int> d(a, b);
    return d(rng());
}

static void logLine(std::ofstream& log, const std::string& state, int page = -1) {
    DWORD t = timeGetTime();
    log << t << ";" << GetCurrentProcessId() << ";" << state;
    if (page >= 0) log << ";page=" << page;
    log << "\n";
    log.flush();
}

static bool initShared(SharedContext& ctx) {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    ctx.pageSize = si.dwPageSize;

    ctx.hInitMutex = CreateMutexW(nullptr, FALSE, INIT_MUTEX_NAME);
    if (!ctx.hInitMutex) return false;

    WaitForSingleObject(ctx.hInitMutex, INFINITE);

    ctx.totalSize = sizeof(SharedHeader) + ctx.pageSize * PAGES_COUNT;

    ctx.hMap = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        ctx.totalSize,
        MAPPING_NAME
    );
    if (!ctx.hMap) {
        ReleaseMutex(ctx.hInitMutex);
        return false;
    }

    bool firstCreate = (GetLastError() != ERROR_ALREADY_EXISTS);

    ctx.base = (LPBYTE)MapViewOfFile(ctx.hMap, FILE_MAP_ALL_ACCESS, 0, 0, ctx.totalSize);
    if (!ctx.base) {
        ReleaseMutex(ctx.hInitMutex);
        CloseHandle(ctx.hMap);
        return false;
    }

    ctx.hdr = reinterpret_cast<SharedHeader*>(ctx.base);
    ctx.pages = reinterpret_cast<char*>(ctx.base + sizeof(SharedHeader));

    if (firstCreate || ctx.hdr->magic != 0x1234ABCD) {
        ZeroMemory(ctx.base, ctx.totalSize);
        ctx.hdr->magic = 0x1234ABCD;
        ctx.hdr->pageSize = ctx.pageSize;
        ctx.hdr->pagesCount = PAGES_COUNT;

        for (int i = 0; i < PAGES_COUNT; ++i) {
            std::ostringstream ss;
            ss << "EMPTY PAGE " << i;
            std::string s = ss.str();
            memcpy(ctx.pages + i * ctx.pageSize, s.c_str(), s.size() + 1);
            ctx.hdr->readerCounts[i] = 0;
        }
    }

    // Блокируем страницы в RAM
    VirtualLock(ctx.base, ctx.totalSize);

    ReleaseMutex(ctx.hInitMutex);
    return true;
}

static void closeShared(SharedContext& ctx) {
    if (ctx.base) {
        VirtualUnlock(ctx.base, ctx.totalSize);
        UnmapViewOfFile(ctx.base);
    }
    if (ctx.hMap) CloseHandle(ctx.hMap);
    if (ctx.hInitMutex) CloseHandle(ctx.hInitMutex);
}

int main(int argc, char* argv[]) {
    int iterations = 20;
    if (argc > 1) iterations = atoi(argv[1]);

    std::ofstream log("reader_" + std::to_string(GetCurrentProcessId()) + ".log", std::ios::app);
    log << "time;pid;state;info\n";

    SharedContext ctx;
    if (!initShared(ctx)) {
        std::cerr << "initShared failed\n";
        return 1;
    }

    std::vector<HANDLE> rcMutex(PAGES_COUNT), wrtSem(PAGES_COUNT);
    for (int i = 0; i < PAGES_COUNT; ++i) {
        rcMutex[i] = CreateMutexW(nullptr, FALSE, RcMutexName(i).c_str());
        wrtSem[i] = CreateSemaphoreW(nullptr, 1, 1, WrtSemaphoreName(i).c_str());
        if (!rcMutex[i] || !wrtSem[i]) {
            std::cerr << "sync object create/open failed\n";
            closeShared(ctx);
            return 2;
        }
    }

    for (int k = 0; k < iterations; ++k) {
        int page = rndInt(0, PAGES_COUNT - 1);

        logLine(log, "WAIT_READ_BEGIN", page);

        WaitForSingleObject(rcMutex[page], INFINITE);
        LONG rc = ++ctx.hdr->readerCounts[page];
        if (rc == 1) {
            WaitForSingleObject(wrtSem[page], INFINITE);
        }
        ReleaseMutex(rcMutex[page]);

        logLine(log, "READING", page);

        int delayMs = rndInt(500, 1500);
        Sleep(delayMs);

        char* pagePtr = ctx.pages + page * ctx.pageSize;
        std::string text(pagePtr);
        std::cout << "[Reader " << GetCurrentProcessId() << "] page " << page
                  << " => " << text << "\n";

        logLine(log, "RELEASE_READ_BEGIN", page);

        WaitForSingleObject(rcMutex[page], INFINITE);
        rc = --ctx.hdr->readerCounts[page];
        if (rc == 0) {
            ReleaseSemaphore(wrtSem[page], 1, nullptr);
        }
        ReleaseMutex(rcMutex[page]);

        logLine(log, "IDLE", page);
        Sleep(rndInt(200, 700));
    }

    for (int i = 0; i < PAGES_COUNT; ++i) {
        CloseHandle(rcMutex[i]);
        CloseHandle(wrtSem[i]);
    }

    closeShared(ctx);
    return 0;
}
