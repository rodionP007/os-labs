// lab2_1_virtual_memory.cpp


#define NOMINMAX
#include <windows.h>

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <locale.h>

static void clearScreen() {
    system("cls");
}

const char* stateToRu(DWORD state) {
    switch (state) {
        case MEM_COMMIT:  return "Выделено (MEM_COMMIT)";
        case MEM_RESERVE: return "Зарезервировано (MEM_RESERVE)";
        case MEM_FREE:    return "Свободно (MEM_FREE)";
        default:          return "Неизвестно";
    }
}

const char* typeToRu(DWORD type) {
    switch (type) {
        case MEM_PRIVATE: return "Частная память процесса (MEM_PRIVATE)";
        case MEM_MAPPED:  return "Отображаемая память (MEM_MAPPED)";
        case MEM_IMAGE:   return "Образ модуля (MEM_IMAGE)";
        default:          return "Неизвестно";
    }
}

const char* protectToRu(DWORD protect) {
    switch (protect) {
        case 0:                       return "Не задана";
        case PAGE_NOACCESS:           return "Нет доступа (PAGE_NOACCESS)";
        case PAGE_READONLY:           return "Только чтение (PAGE_READONLY)";
        case PAGE_READWRITE:          return "Чтение/запись (PAGE_READWRITE)";
        case PAGE_WRITECOPY:          return "Копирование при записи (PAGE_WRITECOPY)";
        case PAGE_EXECUTE:            return "Выполнение (PAGE_EXECUTE)";
        case PAGE_EXECUTE_READ:       return "Выполнение/чтение (PAGE_EXECUTE_READ)";
        case PAGE_EXECUTE_READWRITE:  return "Выполнение/чтение/запись (PAGE_EXECUTE_READWRITE)";
        case PAGE_EXECUTE_WRITECOPY:  return "Выполнение/копирование при записи (PAGE_EXECUTE_WRITECOPY)";
        default:                      return "Другая защита";
    }
}

const char* archToRu(WORD arch) {
    switch (arch) {
        case PROCESSOR_ARCHITECTURE_INTEL: return "x86";
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64 (AMD64)";
        case PROCESSOR_ARCHITECTURE_ARM:   return "ARM";
        default:                           return "Неизвестно";
    }
}

static void header(const std::string& title) {
    clearScreen();
    std::cout << "========================================\n";
    std::cout << title << "\n";
    std::cout << "========================================\n\n";
}

// -------------------- helpers: input --------------------
static std::string readLine(const std::string& prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

static void pauseEnter() {
    std::cout << "\nНажмите Enter...";
    std::string tmp;
    std::getline(std::cin, tmp);
}

static bool parseSize(const std::string& s, SIZE_T& out) {
    // поддержка: 123, 123K, 123M, 123G (K/M/G = 1024^n)
    std::string t;
    for (char c : s) if (!std::isspace((unsigned char)c)) t.push_back(c);
    if (t.empty()) return false;

    char suf = 0;
    if (std::isalpha((unsigned char)t.back())) {
        suf = (char)std::toupper((unsigned char)t.back());
        t.pop_back();
    }
    if (t.empty()) return false;

    unsigned long long v = 0;
    try {
        v = std::stoull(t);
    } catch (...) { return false; }

    unsigned long long mul = 1;
    if (suf == 'K') mul = 1024ull;
    else if (suf == 'M') mul = 1024ull * 1024ull;
    else if (suf == 'G') mul = 1024ull * 1024ull * 1024ull;
    else if (suf != 0) return false;

    unsigned long long r = v * mul;
    out = (SIZE_T)r;
    return true;
}

static bool parsePtr(const std::string& s, void*& out) {
    // принимает: 0x..., либо просто hex/dec
    std::string t;
    for (char c : s) if (!std::isspace((unsigned char)c)) t.push_back(c);
    if (t.empty()) return false;

    unsigned long long v = 0;
    try {
        size_t idx = 0;
        int base = 10;
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) base = 16;
        else {
            // если есть буквы A-F -> hex
            for (char c : t) {
                if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) { base = 16; break; }
            }
        }
        v = std::stoull(t, &idx, base);
        if (idx != t.size()) return false;
    } catch (...) { return false; }

    out = (void*)(uintptr_t)v;
    return true;
}

static DWORD parseProtectChoice() {
    std::cout << "\nВыберите защиту:\n"
              << "  1) PAGE_NOACCESS               - доступ запрещен\n"
              << "  2) PAGE_READONLY               - только чтение\n"
              << "  3) PAGE_READWRITE              - чтение и запись\n"
              << "  4) PAGE_WRITECOPY              - копирование при записи\n"
              << "  5) PAGE_EXECUTE                - только выполнение\n"
              << "  6) PAGE_EXECUTE_READ           - выполнение и чтение\n"
              << "  7) PAGE_EXECUTE_READWRITE      - выполнение, чтение и запись\n"
              << "  8) PAGE_EXECUTE_WRITECOPY      - выполнение и копирование при записи\n"
              << "  9) PAGE_READWRITE | PAGE_GUARD - чтение/запись + сторожевая страница\n"
              << " 10) PAGE_READONLY  | PAGE_GUARD - только чтение + сторожевая страница\n";
    std::string s = readLine("Введите номер: ");
    int n = 0;
    try { n = std::stoi(s); } catch (...) { return 0; }

    switch (n) {
        case 1:  return PAGE_NOACCESS;
        case 2:  return PAGE_READONLY;
        case 3:  return PAGE_READWRITE;
        case 4:  return PAGE_WRITECOPY;
        case 5:  return PAGE_EXECUTE;
        case 6:  return PAGE_EXECUTE_READ;
        case 7:  return PAGE_EXECUTE_READWRITE;
        case 8:  return PAGE_EXECUTE_WRITECOPY;
        case 9:  return PAGE_READWRITE | PAGE_GUARD;
        case 10: return PAGE_READONLY  | PAGE_GUARD;
        default: return 0;
    }
}

// -------------------- helpers: errors --------------------
static void printLastError(const char* where) {
    DWORD err = GetLastError();
    LPSTR msg = nullptr;
    DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    FormatMessageA(flags, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&msg, 0, nullptr);

    std::cerr << "\n[ОШИБКА] " << where << " -> code=" << err;
    if (msg) std::cerr << " (" << msg << ")";
    std::cerr << "\n";
    if (msg) LocalFree(msg);
}
static std::string protectToStr(DWORD p) {
    if (p == 0) return "Не задана";

    struct Item {
        DWORD bit;
        const char* name;
        const char* desc;
    };

    static const Item base[] = {
        { PAGE_NOACCESS,          "PAGE_NOACCESS",          "доступ запрещен" },
        { PAGE_READONLY,          "PAGE_READONLY",          "только чтение" },
        { PAGE_READWRITE,         "PAGE_READWRITE",         "чтение и запись" },
        { PAGE_WRITECOPY,         "PAGE_WRITECOPY",         "копирование при записи" },
        { PAGE_EXECUTE,           "PAGE_EXECUTE",           "только выполнение" },
        { PAGE_EXECUTE_READ,      "PAGE_EXECUTE_READ",      "выполнение и чтение" },
        { PAGE_EXECUTE_READWRITE, "PAGE_EXECUTE_READWRITE", "выполнение, чтение и запись" },
        { PAGE_EXECUTE_WRITECOPY, "PAGE_EXECUTE_WRITECOPY", "выполнение и копирование при записи" },
    };

    static const Item mods[] = {
        { PAGE_GUARD,        "PAGE_GUARD",        "сторожевая страница" },
        { PAGE_NOCACHE,      "PAGE_NOCACHE",      "без кэширования" },
        { PAGE_WRITECOMBINE, "PAGE_WRITECOMBINE", "объединение операций записи" },
    };

    std::ostringstream oss;
    bool first = true;
    bool foundBase = false;

    for (const auto& it : base) {
        if ((p & 0xFFu) == it.bit) {
            oss << it.name << " (" << it.desc << ")";
            first = false;
            foundBase = true;
            break;
        }
    }

    if (!foundBase) {
        oss << "0x" << std::hex << std::uppercase << p << " (неизвестная защита)";
        first = false;
    }

    for (const auto& it : mods) {
        if (p & it.bit) {
            if (!first) oss << " | ";
            oss << it.name << " (" << it.desc << ")";
            first = false;
        }
    }

    return oss.str();
}

// -------------------- global state for allocated region --------------------
static void*  g_base = nullptr;
static SIZE_T g_size = 0;

// -------------------- menu actions --------------------
static void menuSystemInfo() {
    header("1) GetSystemInfo");
    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    std::cout << "\n--- GetSystemInfo ---\n";
    std::cout << "Архитектура процессора: "
          << archToRu(si.wProcessorArchitecture)
          << " (" << si.wProcessorArchitecture << ")\n";
    std::cout << "Размер страницы (dwPageSize): " << si.dwPageSize << " bytes\n";
    std::cout << "Гранулярность выделения (dwAllocationGranularity): " << si.dwAllocationGranularity << " bytes\n";
    std::cout << "Мин. адрес приложения: " << si.lpMinimumApplicationAddress << "\n";
    std::cout << "Макс. адрес приложения: " << si.lpMaximumApplicationAddress << "\n";
    std::cout << "Число процессоров: " << si.dwNumberOfProcessors << "\n";

    pauseEnter();
}

static void menuMemoryStatus() {
    header("2) MemoryStatus");
    MEMORYSTATUSEX st{};
    st.dwLength = sizeof(st);
    if (!GlobalMemoryStatusEx(&st)) {
        printLastError("GlobalMemoryStatusEx");
        pauseEnter();
        return;
    }

    auto mib = [](DWORDLONG b) { return (double)b / (1024.0 * 1024.0); };

    std::cout << "\n--- GlobalMemoryStatusEx ---\n";
    std::cout << "Загрузка памяти: " << st.dwMemoryLoad << " %\n";
    std::cout << "Общий объем физической памяти:  " << mib(st.ullTotalPhys) << " MiB\n";
    std::cout << "Доступный объем физической памяти:  " << mib(st.ullAvailPhys) << " MiB\n";
    std::cout << "Общий объем файла подкачки: " << mib(st.ullTotalPageFile) << " MiB\n";
    std::cout << "Доступный объем файла подкачки: " << mib(st.ullAvailPageFile) << " MiB\n";
    std::cout << "Общий объем виртуальной памяти процесса:  " << mib(st.ullTotalVirtual) << " MiB\n";
    std::cout << "Доступный объем виртуальной памяти процесса:  " << mib(st.ullAvailVirtual) << " MiB\n";

    pauseEnter();
}

static void printVirtualQuery(void* addr) {
    MEMORY_BASIC_INFORMATION mbi{};
    SIZE_T r = VirtualQuery(addr, &mbi, sizeof(mbi));
    if (r == 0) {
        printLastError("VirtualQuery");
        return;
    }

    std::cout << "\n--- VirtualQuery ---\n";
    std::cout << "Адрес:        " << addr << "\n";
    std::cout << "Базовый адрес региона:  " << mbi.BaseAddress << "\n";
    std::cout << "Базовый адрес выделения:" << mbi.AllocationBase << "\n";
    std::cout << "Размер региона:         " << mbi.RegionSize << " байт\n";
    std::cout << "Состояние:              " << stateToRu(mbi.State) << "\n";

    if (mbi.State == MEM_FREE) {
        std::cout << "Текущая защита:         Не задана\n";
        std::cout << "Защита при выделении:   Не задана\n";
        std::cout << "Тип памяти:             Не задан\n";
    } else {
        std::cout << "Текущая защита:         " << protectToRu(mbi.Protect) << "\n";
        std::cout << "Защита при выделении:   " << protectToRu(mbi.AllocationProtect) << "\n";
        std::cout << "Тип памяти:             " << typeToRu(mbi.Type) << "\n";
    }
}

static void menuVirtualQuery() {
    header("3) VirtualQuery");
    std::string s = readLine("Введите адрес (hex, напр. 0x7FF6...): ");
    void* addr = nullptr;
    if (!parsePtr(s, addr)) {
        std::cout << "Неверный адрес.\n";
        pauseEnter();
        return;
    }
    printVirtualQuery(addr);
    pauseEnter();
}

static bool readSizeForRegion(SIZE_T& sizeOut) {
    std::string s = readLine("Введите размер региона (например 4096, 64K, 1M): ");
    SIZE_T sz = 0;
    if (!parseSize(s, sz) || sz == 0) {
        std::cout << "Неверный размер.\n";
        return false;
    }
    sizeOut = sz;
    return true;
}

static void menuReserveAuto() {
    header("4) VirtualAlloc: MEM_RESERVE (auto)");
    if (g_base) {
        std::cout << "Уже есть активный регион: base=" << g_base << ", size=" << g_size << "\n";
        std::cout << "Сначала освободите (VirtualFree MEM_RELEASE).\n";
        pauseEnter();
        return;
    }
    SIZE_T sz = 0;
    if (!readSizeForRegion(sz)) { pauseEnter(); return; }

    void* p = VirtualAlloc(nullptr, sz, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) {
        printLastError("VirtualAlloc(MEM_RESERVE)");
        pauseEnter();
        return;
    }
    g_base = p;
    g_size = sz;

    std::cout << "OK: зарезервировано.\nbase=" << g_base << " size=" << g_size << "\n";
    printVirtualQuery(g_base);

    pauseEnter();
}

static void menuReserveAtAddress() {
    header("5) VirtualAlloc: MEM_RESERVE (ввод адреса)");
    if (g_base) {
        std::cout << "Уже есть активный регион: base=" << g_base << ", size=" << g_size << "\n";
        std::cout << "Сначала освободите (VirtualFree MEM_RELEASE).\n";
        pauseEnter();
        return;
    }

    SIZE_T sz = 0;
    if (!readSizeForRegion(sz)) { pauseEnter(); return; }

    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    std::string s = readLine("Введите желаемый базовый адрес (hex): ");
    void* base = nullptr;
    if (!parsePtr(s, base)) {
        std::cout << "Неверный адрес.\n";
        pauseEnter();
        return;
    }

    // Проверим выравнивание по AllocationGranularity
    uintptr_t b = (uintptr_t)base;
    if (b % si.dwAllocationGranularity != 0) {
        std::cout << "Адрес НЕ выровнен по dwAllocationGranularity=" << si.dwAllocationGranularity << ".\n";
        std::cout << "VirtualAlloc скорее всего завершится ошибкой.\n";
    }

    void* p = VirtualAlloc(base, sz, MEM_RESERVE, PAGE_NOACCESS);
    if (!p) {
        printLastError("VirtualAlloc(MEM_RESERVE, base)");
        pauseEnter();
        return;
    }
    g_base = p;
    g_size = sz;

    std::cout << "OK: зарезервировано.\nbase=" << g_base << " size=" << g_size << "\n";
    printVirtualQuery(g_base);

    pauseEnter();
}

static void menuCommitAuto() {
    header("6) VirtualAlloc: MEM_COMMIT (для текущего региона)");
    if (!g_base) {
        std::cout << "Нет активного зарезервированного региона. Сначала RESERVE.\n";
        pauseEnter();
        return;
    }
    // commit весь регион
    void* p = VirtualAlloc(g_base, g_size, MEM_COMMIT, PAGE_READWRITE);
    if (!p) {
        printLastError("VirtualAlloc(MEM_COMMIT)");
        pauseEnter();
        return;
    }
    std::cout << "OK: выполнен COMMIT для base=" << g_base << " size=" << g_size << "\n";
    printVirtualQuery(g_base);
    pauseEnter();
}

static void menuReserveCommitAuto() {
    header("7) VirtualAlloc: MEM_RESERVE|MEM_COMMIT (auto)");
    if (g_base) {
        std::cout << "Уже есть активный регион: base=" << g_base << ", size=" << g_size << "\n";
        std::cout << "Сначала освободите (VirtualFree MEM_RELEASE).\n";
        pauseEnter();
        return;
    }
    SIZE_T sz = 0;
    if (!readSizeForRegion(sz)) { pauseEnter(); return; }

    void* p = VirtualAlloc(nullptr, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) {
        printLastError("VirtualAlloc(MEM_RESERVE|MEM_COMMIT)");
        pauseEnter();
        return;
    }
    g_base = p;
    g_size = sz;

    std::cout << "OK: RESERVE|COMMIT.\nbase=" << g_base << " size=" << g_size << "\n";
    printVirtualQuery(g_base);

    pauseEnter();
}

static void menuReserveCommitAtAddress() {
    header("8) VirtualAlloc: MEM_RESERVE|MEM_COMMIT (ввод адреса)");
    if (g_base) {
        std::cout << "Уже есть активный регион: base=" << g_base << ", size=" << g_size << "\n";
        std::cout << "Сначала освободите (VirtualFree MEM_RELEASE).\n";
        pauseEnter();
        return;
    }

    SIZE_T sz = 0;
    if (!readSizeForRegion(sz)) { pauseEnter(); return; }

    SYSTEM_INFO si{};
    GetSystemInfo(&si);

    std::string s = readLine("Введите желаемый базовый адрес (hex): ");
    void* base = nullptr;
    if (!parsePtr(s, base)) {
        std::cout << "Неверный адрес.\n";
        pauseEnter();
        return;
    }

    uintptr_t b = (uintptr_t)base;
    if (b % si.dwAllocationGranularity != 0) {
        std::cout << "Адрес НЕ выровнен по dwAllocationGranularity=" << si.dwAllocationGranularity << ".\n";
        std::cout << "VirtualAlloc может завершиться ошибкой.\n";
    }

    void* p = VirtualAlloc(base, sz, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!p) {
        printLastError("VirtualAlloc(MEM_RESERVE|MEM_COMMIT, base)");
        pauseEnter();
        return;
    }
    g_base = p;
    g_size = sz;

    std::cout << "OK: RESERVE|COMMIT.\nbase=" << g_base << " size=" << g_size << "\n";
    printVirtualQuery(g_base);

    pauseEnter();
}

static void menuWriteMemory() {
    header("9) Запись DWORD по адресу");
    std::cout << "\n--- Запись DWORD по адресу ---\n";
    if (g_base) {
        std::cout << "Текущий регион: base=" << g_base << " size=" << g_size << "\n";
    }

    std::string sAddr = readLine("Введите адрес назначения (hex): ");
    void* addr = nullptr;
    if (!parsePtr(sAddr, addr)) {
        std::cout << "Неверный адрес.\n";
        pauseEnter();
        return;
    }

    std::string sVal = readLine("Введите значение DWORD (например: 1234 или 0x1234): ");
    unsigned long v = 0;
    try {
        size_t idx = 0;
        int base = 10;
        std::string t;
        for (char c : sVal) if (!std::isspace((unsigned char)c)) t.push_back(c);
        if (t.size() > 2 && t[0] == '0' && (t[1] == 'x' || t[1] == 'X')) base = 16;
        else {
            for (char c : t) {
                if ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')) { base = 16; break; }
            }
        }
        v = std::stoul(t, &idx, base);
        if (idx != t.size()) throw std::runtime_error("bad");
    } catch (...) {
        std::cout << "Неверное значение.\n";
        pauseEnter();
        return;
    }

    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQuery(addr, &mbi, sizeof(mbi))) {
        printLastError("VirtualQuery(before write)");
        pauseEnter();
        return;
    }

    std::cout << "Статус: " << stateToRu(mbi.State)
              << ", Protect: " << protectToStr(mbi.Protect) << "\n";

    if (mbi.State != MEM_COMMIT) {
        std::cout << "Нельзя выполнять запись: регион только зарезервирован и не переведен в состояние MEM_COMMIT.\n";
        pauseEnter();
        return;
    }

    DWORD p = mbi.Protect & 0xFFu;
    bool canWrite =
        (p == PAGE_READWRITE) ||
        (p == PAGE_WRITECOPY) ||
        (p == PAGE_EXECUTE_READWRITE) ||
        (p == PAGE_EXECUTE_WRITECOPY);

    if (!canWrite) {
        std::cout << "Защита не разрешает запись. Используйте VirtualProtect.\n";
        pauseEnter();
        return;
    }

    // Пишем. Если адрес валидный и страница COMMIT+WRITE — это безопасно.
    *(volatile DWORD*)addr = (DWORD)v;

    std::cout << "OK: записано.\n";
    std::cout << "Прочитано обратно: " << std::hex << std::showbase
              << *(volatile DWORD*)addr << std::dec << std::noshowbase << "\n";

    pauseEnter();
}

static void menuProtect() {
    header("10) VirtualProtect");
    std::cout << "\n--- VirtualProtect ---\n";
    std::cout << "Если region выделен: base=" << g_base << " size=" << g_size << "\n";

    std::string sBase = readLine("Введите базовый адрес (hex): ");
    void* base = nullptr;
    if (!parsePtr(sBase, base)) {
        std::cout << "Неверный адрес.\n";
        pauseEnter();
        return;
    }

    SIZE_T sz = 0;
    if (!readSizeForRegion(sz)) { pauseEnter(); return; }

    DWORD np = parseProtectChoice();
    if (np == 0) {
        std::cout << "Неверный выбор защиты.\n";
        pauseEnter();
        return;
    }

    DWORD oldp = 0;
    if (!VirtualProtect(base, sz, np, &oldp)) {
        printLastError("VirtualProtect");
        pauseEnter();
        return;
    }

    std::cout << "OK: защита изменена.\n";
    std::cout << "OldProtect: " << protectToStr(oldp) << "\n";

    // Проверка: просто выводим, что VirtualQuery теперь показывает новую защиту
    printVirtualQuery(base);


    pauseEnter();
}

static void menuDecommit() {
    header("11) VirtualFree: MEM_DECOMMIT");
    if (!g_base) {
        std::cout << "Нет активного региона.\n";
        pauseEnter();
        return;
    }

    std::cout << "\n--- VirtualFree MEM_DECOMMIT ---\n";
    std::cout << "Текущий регион: base=" << g_base << " size=" << g_size << "\n";
    std::cout << "Это снимет COMMIT (физическую память), но оставит RESERVE.\n";

    if (!VirtualFree(g_base, g_size, MEM_DECOMMIT)) {
        printLastError("VirtualFree(MEM_DECOMMIT)");
        pauseEnter();
        return;
    }

    std::cout << "OK: decommit выполнен.\n";
    printVirtualQuery(g_base);

    pauseEnter();
}

static void menuRelease() {
    header("12) VirtualFree: MEM_RELEASE");
    if (!g_base) {
        std::cout << "Нет активного региона.\n";
        pauseEnter();
        return;
    }

    std::cout << "\n--- VirtualFree MEM_RELEASE ---\n";
    std::cout << "Текущий регион: base=" << g_base << " size=" << g_size << "\n";
    std::cout << "Это полностью освободит регион (и RESERVE, и COMMIT).\n";

    if (!VirtualFree(g_base, 0, MEM_RELEASE)) {
        printLastError("VirtualFree(MEM_RELEASE)");
        pauseEnter();
        return;
    }
    std::cout << "OK: регион освобождён.\n";

    printVirtualQuery(g_base);
    g_base = nullptr;
    g_size = 0;

    pauseEnter();
}

static void printMenu() {
    std::cout << "\n====================================================================\n";
    std::cout << "Работа 2. Управление памятью. Задание 2.1\n";
    std::cout << "Исследование виртуального адресного пространства процесса\n";
    std::cout << "Активный регион: base=" << g_base
              << " size=" << (unsigned long long)g_size << "\n";
    std::cout << "--------------------------------------------------------------------\n";
    std::cout << " 1) GetSystemInfo - получение информации о вычислительной системе\n";
    std::cout << " 2) GlobalMemoryStatusEx - определение статуса виртуальной памяти\n";
    std::cout << " 3) VirtualQuery - определение состояния участка памяти по адресу\n";
    std::cout << " 4) VirtualAlloc: MEM_RESERVE (auto) - раздельное резервирование региона\n";
    std::cout << " 5) VirtualAlloc: MEM_RESERVE (ввод адреса) - резервирование региона по введенному адресу\n";
    std::cout << " 6) VirtualAlloc: MEM_COMMIT - передача физической памяти зарезервированному региону\n";
    std::cout << " 7) VirtualAlloc: MEM_RESERVE | MEM_COMMIT (auto) - одновременное резервирование и выделение памяти\n";
    std::cout << " 8) VirtualAlloc: MEM_RESERVE | MEM_COMMIT (ввод адреса) - то же по введенному адресу\n";
    std::cout << " 9) Запись DWORD по адресу - запись данных в ячейку памяти\n";
    std::cout << "10) VirtualProtect - установка и проверка защиты доступа к памяти\n";
    std::cout << "11) VirtualFree: MEM_DECOMMIT - освобождение физической памяти без удаления региона\n";
    std::cout << "12) VirtualFree: MEM_RELEASE - полное освобождение региона памяти\n";
    std::cout << " 0) Выход\n";
    std::cout << "====================================================================\n";
}

int main() {
    setlocale(LC_ALL, "");
    while (true) {
        clearScreen();
        printMenu();
        std::string s = readLine("Выберите пункт: ");
        int c = -1;
        try { c = std::stoi(s); } catch (...) { c = -1; }

        switch (c) {
            case 1:  menuSystemInfo(); break;
            case 2:  menuMemoryStatus(); break;
            case 3:  menuVirtualQuery(); break;
            case 4:  menuReserveAuto(); break;
            case 5:  menuReserveAtAddress(); break;
            case 6:  menuCommitAuto(); break;
            case 7:  menuReserveCommitAuto(); break;
            case 8:  menuReserveCommitAtAddress(); break;
            case 9:  menuWriteMemory(); break;
            case 10: menuProtect(); break;
            case 11: menuDecommit(); break;
            case 12: menuRelease(); break;
            case 0:
                // аккуратно освободим, если забыли
                if (g_base) VirtualFree(g_base, 0, MEM_RELEASE);
                return 0;
            default:
                std::cout << "Неверный пункт.\n";
                pauseEnter();
                break;
        }
    }
}
