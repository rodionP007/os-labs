// main.cpp (Code::Blocks + MinGW, Windows)
#define NOMINMAX
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>
#include <iomanip>
#include <locale.h>
static void printLastError(const char* where) {
    DWORD err = GetLastError();
    char* msg = nullptr;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        err,
        MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT), // русский язык
        (LPSTR)&msg,
        0,
        nullptr
    );

    std::cerr << "[ОШИБКА] В функции \"" << where << "\"\n";
    std::cerr << "Код ошибки: " << err << "\n";
    std::cerr << "Описание: " << (msg ? msg : "Не удалось получить описание ошибки") << "\n";

    if (msg)
        LocalFree(msg);
}


static std::string readLine(const char* prompt) {
    std::cout << prompt;
    std::string s;
    std::getline(std::cin, s);
    return s;
}

static void listDisks_GetLogicalDrives() {
    DWORD mask = GetLogicalDrives();

    if (mask == 0) {
        printLastError("GetLogicalDrives");
        return;
    }

    std::cout << "Список логических дисков (GetLogicalDrives)\n";
    std::cout << "--------------------------------------------\n";

    int count = 0;

    for (int i = 0; i < 26; ++i) {
        if (mask & (1u << i)) {
            char letter = char('A' + i);
            std::cout << "Диск: " << letter << ":\\\n";
            count++;
        }
    }

    if (count == 0) {
        std::cout << "Логические диски не найдены.\n";
    } else {
        std::cout << "\nВсего найдено дисков: " << count << "\n";
    }
}


static void listDisks_GetLogicalDriveStrings() {

    DWORD needed = GetLogicalDriveStringsA(0, nullptr);
    if (needed == 0) {
        printLastError("GetLogicalDriveStringsA (получение размера)");
        return;
    }

    std::vector<char> buf(needed + 1, '\0');

    DWORD got = GetLogicalDriveStringsA(needed, buf.data());
    if (got == 0) {
        printLastError("GetLogicalDriveStringsA");
        return;
    }

    std::cout << "Список логических дисков (GetLogicalDriveStrings)\n";
    std::cout << "--------------------------------------------------\n";

    int count = 0;

    for (char* p = buf.data(); *p; p += strlen(p) + 1) {
        std::cout << "Диск: " << p << "\n";
        count++;
    }

    if (count == 0) {
        std::cout << "Диски не найдены.\n";
    } else {
        std::cout << "\nВсего найдено дисков: " << count << "\n";
    }
}


static const char* driveTypeToStr(UINT t) {
    switch (t) {
        case DRIVE_UNKNOWN:     return "DRIVE_UNKNOWN";
        case DRIVE_NO_ROOT_DIR: return "DRIVE_NO_ROOT_DIR";
        case DRIVE_REMOVABLE:   return "DRIVE_REMOVABLE";
        case DRIVE_FIXED:       return "DRIVE_FIXED";
        case DRIVE_REMOTE:      return "DRIVE_REMOTE";
        case DRIVE_CDROM:       return "DRIVE_CDROM";
        case DRIVE_RAMDISK:     return "DRIVE_RAMDISK";
        default:                return "(?)";
    }
}

static void diskInfoAndFreeSpace() {

    std::string root = readLine("Введите диск (пример: C:\\): ");
    if (root.empty()) return;

    UINT dtype = GetDriveTypeA(root.c_str());

    std::cout << "\nИнформация о диске\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Тип диска: " << driveTypeToStr(dtype) << "\n";

    char volumeName[MAX_PATH] = {};
    char fsName[MAX_PATH] = {};
    DWORD serial = 0, maxCompLen = 0, fsFlags = 0;

    if (!GetVolumeInformationA(
        root.c_str(),
        volumeName, MAX_PATH,
        &serial,
        &maxCompLen,
        &fsFlags,
        fsName, MAX_PATH
    )) {
        printLastError("GetVolumeInformationA");
        return;
    }

    std::cout << "Метка тома:              " << volumeName << "\n";
    std::cout << "Файловая система:        " << fsName << "\n";
    std::cout << "Серийный номер тома:     0x"
              << std::hex << serial << std::dec << "\n";
    std::cout << "Макс. длина имени файла: " << maxCompLen << "\n";

    DWORD spc = 0, bps = 0, freeCl = 0, totalCl = 0;

    if (!GetDiskFreeSpaceA(root.c_str(), &spc, &bps, &freeCl, &totalCl)) {
        printLastError("GetDiskFreeSpaceA");
        return;
    }

    unsigned long long clusterSize =
        (unsigned long long)spc * bps;

    unsigned long long totalBytes =
        (unsigned long long)totalCl * clusterSize;

    unsigned long long freeBytes =
        (unsigned long long)freeCl * clusterSize;

    std::cout << "\nРазмер кластера: " << clusterSize << " байт\n";
    std::cout << "Общий объём:     " << totalBytes  << " байт\n";
    std::cout << "Свободно:        " << freeBytes   << " байт\n";
}


static void createDirectoryMenu() {

    std::string path = readLine("Введите путь к каталогу для создания: ");
    if (path.empty()) return;

    if (CreateDirectoryA(path.c_str(), nullptr)) {
        std::cout << "\nКаталог успешно создан.\n";
        std::cout << "Путь: " << path << "\n";
    } else {
        printLastError("CreateDirectoryA");
    }
}


static void removeDirectoryMenu() {

    std::string path = readLine("Введите путь к каталогу для удаления (каталог должен быть пустым): ");
    if (path.empty()) return;

    if (RemoveDirectoryA(path.c_str())) {
        std::cout << "\nКаталог успешно удалён.\n";
        std::cout << "Путь: " << path << "\n";
    } else {
        printLastError("RemoveDirectoryA");
    }
}


static void createFileInDirectory() {

    std::string dir  = readLine("Введите каталог (пример: C:\\lab\\dir1): ");
    std::string name = readLine("Введите имя файла (пример: test.txt): ");

    if (dir.empty() || name.empty()) return;

    std::string full = dir;
    if (!full.empty() && full.back() != '\\')
        full += "\\";

    full += name;

    HANDLE h = CreateFileA(
        full.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,               // ошибка если файл существует
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        printLastError("CreateFileA (CREATE_NEW)");
        return;
    }

    const char* text = "Файл создан с использованием Win32 CreateFile.\r\n";

    DWORD written = 0;
    WriteFile(h, text, (DWORD)strlen(text), &written, nullptr);

    CloseHandle(h);

    std::cout << "\nФайл успешно создан.\n";
    std::cout << "Полный путь: " << full << "\n";
}

static void copyFileMenu() {

    std::string src = readLine("Введите полный путь к исходному файлу: ");
    std::string dst = readLine("Введите полный путь к файлу назначения: ");

    if (src.empty() || dst.empty()) return;

    // TRUE — запретить перезапись существующего файла
    if (CopyFileA(src.c_str(), dst.c_str(), TRUE)) {

        std::cout << "\nФайл успешно скопирован.\n";
        std::cout << "Из:  " << src << "\n";
        std::cout << "В:   " << dst << "\n";

    } else {

        DWORD err = GetLastError();

        if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {

            std::cout << "\nОшибка: файл с таким именем уже существует в папке назначения.\n";
            std::cout << "Копирование отменено.\n";

        } else {

            printLastError("CopyFileA");
        }
    }
}


static void moveFileMenu() {

    std::string src = readLine("Введите полный путь к исходному файлу: ");
    std::string dst = readLine("Введите полный путь к файлу назначения: ");

    if (src.empty() || dst.empty()) return;

    // MoveFile не перезаписывает существующий файл
    if (MoveFileA(src.c_str(), dst.c_str())) {

        std::cout << "\nФайл успешно перемещён.\n";
        std::cout << "Из:  " << src << "\n";
        std::cout << "В:   " << dst << "\n";

    } else {

        DWORD err = GetLastError();

        if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS) {

            std::cout << "\nОшибка: файл с таким именем уже существует в папке назначения.\n";
            std::cout << "Перезапись запрещена.\n";
            std::cout << "Для перезаписи используйте MoveFileEx с флагом MOVEFILE_REPLACE_EXISTING.\n";

        } else {

            printLastError("MoveFileA");
        }
    }
}


static void moveFileExMenu() {

    std::string src = readLine("Введите полный путь к исходному файлу: ");
    std::string dst = readLine("Введите полный путь к файлу назначения: ");

    if (src.empty() || dst.empty()) return;

    std::cout << "\nВыберите режим перемещения:\n"
              << " 1) Без флагов (обычное перемещение)\n"
              << " 2) С перезаписью (MOVEFILE_REPLACE_EXISTING)\n"
              << " 3) Разрешить копирование между дисками (MOVEFILE_COPY_ALLOWED)\n"
              << " 4) Перезапись + копирование между дисками\n";

    std::string opt = readLine("Введите номер (1-4): ");

    DWORD flags = 0;

    if (opt == "2")
        flags = MOVEFILE_REPLACE_EXISTING;
    else if (opt == "3")
        flags = MOVEFILE_COPY_ALLOWED;
    else if (opt == "4")
        flags = MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED;

    if (MoveFileExA(src.c_str(), dst.c_str(), flags)) {

        std::cout << "\nОперация MoveFileEx выполнена успешно.\n";
        std::cout << "Из:  " << src << "\n";
        std::cout << "В:   " << dst << "\n";

    } else {

        printLastError("MoveFileExA");
    }
}


static void showAttributes() {

    std::string path = readLine("Введите путь к файлу: ");
    if (path.empty()) return;

    DWORD attr = GetFileAttributesA(path.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES) {
        printLastError("GetFileAttributesA");
        return;
    }

    std::cout << "\nАтрибуты файла\n";
    std::cout << "----------------------------------\n";
    std::cout << "Битовая маска: 0x" << std::hex << attr << std::dec << "\n\n";

    std::cout << "Только чтение (READONLY):  "
              << ((attr & FILE_ATTRIBUTE_READONLY) ? "Да" : "Нет") << "\n";

    std::cout << "Скрытый (HIDDEN):          "
              << ((attr & FILE_ATTRIBUTE_HIDDEN) ? "Да" : "Нет") << "\n";

    std::cout << "Системный (SYSTEM):        "
              << ((attr & FILE_ATTRIBUTE_SYSTEM) ? "Да" : "Нет") << "\n";

    std::cout << "Архивный (ARCHIVE):        "
              << ((attr & FILE_ATTRIBUTE_ARCHIVE) ? "Да" : "Нет") << "\n";

    std::cout << "Временный (TEMPORARY):     "
              << ((attr & FILE_ATTRIBUTE_TEMPORARY) ? "Да" : "Нет") << "\n";
}


static void toggleAttributes() {

    std::string path = readLine("Введите путь к файлу: ");
    if (path.empty()) return;

    DWORD attr = GetFileAttributesA(path.c_str());

    if (attr == INVALID_FILE_ATTRIBUTES) {
        printLastError("GetFileAttributesA");
        return;
    }

    std::cout << "\nВыберите атрибут для изменения:\n"
              << " 1) Только чтение (READONLY)\n"
              << " 2) Скрытый (HIDDEN)\n"
              << " 3) Архивный (ARCHIVE)\n";

    std::string opt = readLine("Введите номер (1-3): ");

    if (opt == "1")
        attr ^= FILE_ATTRIBUTE_READONLY;
    else if (opt == "2")
        attr ^= FILE_ATTRIBUTE_HIDDEN;
    else if (opt == "3")
        attr ^= FILE_ATTRIBUTE_ARCHIVE;
    else {
        std::cout << "Неверный выбор.\n";
        return;
    }

    if (!SetFileAttributesA(path.c_str(), attr)) {
        printLastError("SetFileAttributesA");
        return;
    }

    std::cout << "\nАтрибут успешно изменён.\n";
}


static void printFileTimeLocal(const FILETIME& ft, const char* label) {
    SYSTEMTIME stUTC{}, stLocal{};
    FileTimeToSystemTime(&ft, &stUTC);
    SystemTimeToTzSpecificLocalTime(nullptr, &stUTC, &stLocal);

    std::cout << label << ": "
              << std::setfill('0')
              << std::setw(2) << stLocal.wDay << "."
              << std::setw(2) << stLocal.wMonth << "."
              << std::setw(4) << stLocal.wYear << " "
              << std::setw(2) << stLocal.wHour << ":"
              << std::setw(2) << stLocal.wMinute << ":"
              << std::setw(2) << stLocal.wSecond
              << "\n";
}

static void fileInfoByHandleAndTimes() {

    std::string path = readLine("Введите путь к файлу: ");
    if (path.empty()) return;

    HANDLE h = CreateFileA(
        path.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        printLastError("CreateFileA (OPEN_EXISTING)");
        return;
    }

    BY_HANDLE_FILE_INFORMATION info{};

    if (!GetFileInformationByHandle(h, &info)) {
        printLastError("GetFileInformationByHandle");
        CloseHandle(h);
        return;
    }

    unsigned long long size =
        ((unsigned long long)info.nFileSizeHigh << 32) |
        (unsigned long long)info.nFileSizeLow;

    std::cout << "\nИнформация о файле\n";
    std::cout << "----------------------------------------\n";
    std::cout << "Размер файла:          " << size << " байт\n";
    std::cout << "Количество ссылок:     " << info.nNumberOfLinks << "\n";

    unsigned long long fileIndex =
        ((unsigned long long)info.nFileIndexHigh << 32) |
        info.nFileIndexLow;

    std::cout << "Уникальный индекс:     " << fileIndex << "\n";

    FILETIME creationTime{}, accessTime{}, writeTime{};

    if (GetFileTime(h, &creationTime, &accessTime, &writeTime)) {

        std::cout << "\nВременные метки файла\n";
        std::cout << "----------------------------------------\n";

        printFileTimeLocal(creationTime, "Дата создания");
        printFileTimeLocal(accessTime,   "Последний доступ");
        printFileTimeLocal(writeTime,    "Последнее изменение");

    } else {
        printLastError("GetFileTime");
    }

    CloseHandle(h);
}


static bool parseDT(const std::string& s, SYSTEMTIME& outLocal) {
    // format: DD.MM.YYYY HH:MM:SS (19 chars)
    if (s.size() < 19) return false;
    try {
        outLocal = {};
        outLocal.wDay    = (WORD)std::stoi(s.substr(0, 2));
        outLocal.wMonth  = (WORD)std::stoi(s.substr(3, 2));
        outLocal.wYear   = (WORD)std::stoi(s.substr(6, 4));
        outLocal.wHour   = (WORD)std::stoi(s.substr(11, 2));
        outLocal.wMinute = (WORD)std::stoi(s.substr(14, 2));
        outLocal.wSecond = (WORD)std::stoi(s.substr(17, 2));
        return true;
    } catch (...) {
        return false;
    }
}

static bool localSystemTimeToFileTime(const SYSTEMTIME& local, FILETIME& outFt) {
    FILETIME ftLocal{};
    if (!SystemTimeToFileTime(&local, &ftLocal)) return false;        // трактуем как local
    if (!LocalFileTimeToFileTime(&ftLocal, &outFt)) return false;     // перевод local -> UTC FILETIME
    return true;
}


static void setFileTimeMenu() {

    std::string path = readLine("Введите путь к файлу: ");
    if (path.empty()) return;

    HANDLE h = CreateFileA(
        path.c_str(),
        FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );

    if (h == INVALID_HANDLE_VALUE) {
        printLastError("CreateFileA (FILE_WRITE_ATTRIBUTES)");
        return;
    }

    std::cout << "\nВведите новую дату и время в формате:\n";
    std::cout << "ДД.ММ.ГГГГ ЧЧ:ММ:СС\n\n";

    std::string sC = readLine("Дата создания (Enter — оставить без изменений): ");
    std::string sA = readLine("Дата последнего доступа (Enter — оставить без изменений): ");
    std::string sW = readLine("Дата последнего изменения (Enter — оставить без изменений): ");

    FILETIME ftC{}, ftA{}, ftW{};
    FILETIME *pC = nullptr, *pA = nullptr, *pW = nullptr;

    // Проверка и преобразование даты создания
    if (!sC.empty()) {
        SYSTEMTIME st{};
        if (!parseDT(sC, st) || !localSystemTimeToFileTime(st, ftC)) {
            std::cout << "\nОшибка формата даты создания.\n";
            CloseHandle(h);
            return;
        }
        pC = &ftC;
    }

    // Проверка и преобразование даты доступа
    if (!sA.empty()) {
        SYSTEMTIME st{};
        if (!parseDT(sA, st) || !localSystemTimeToFileTime(st, ftA)) {
            std::cout << "\nОшибка формата даты доступа.\n";
            CloseHandle(h);
            return;
        }
        pA = &ftA;
    }

    // Проверка и преобразование даты изменения
    if (!sW.empty()) {
        SYSTEMTIME st{};
        if (!parseDT(sW, st) || !localSystemTimeToFileTime(st, ftW)) {
            std::cout << "\nОшибка формата даты изменения.\n";
            CloseHandle(h);
            return;
        }
        pW = &ftW;
    }

    if (!SetFileTime(h, pC, pA, pW)) {
        printLastError("SetFileTime");
        CloseHandle(h);
        return;
    }

    std::cout << "\nВременные метки файла успешно обновлены.\n";

    CloseHandle(h);
}


void clearScreen() {
    system("cls");
}

void pauseScreen() {
    std::cout << "\nНажмите Enter чтобы вернуться в меню...";
    std::cin.get();
}

void printHeader(const std::string& title) {
    clearScreen();
    std::cout << "========================================\n";
    std::cout << "  " << title << "\n";
    std::cout << "========================================\n\n";
}

int getIntInput(int min, int max) {
    while (true) {
        std::cout << "Ваш выбор: ";
        std::string input;
        std::getline(std::cin, input);

        std::stringstream ss(input);
        int value;

        if (ss >> value && ss.eof()) {
            if (value >= min && value <= max)
                return value;
        }

        std::cout << "Ошибка ввода. Введите число от "
                  << min << " до " << max << ".\n";
    }
}

void showMenu() {
    std::cout <<
        "1  - Список дисков (GetLogicalDrives)\n"
        "2  - Список дисков (GetLogicalDriveStrings)\n"
        "3  - Информация о диске + свободное место\n"
        "4  - Создать каталог\n"
        "5  - Удалить каталог\n"
        "6  - Создать файл\n"
        "7  - Копировать файл\n"
        "8  - Переместить файл\n"
        "9  - MoveFileEx\n"
        "10 - Показать атрибуты\n"
        "11 - Изменить атрибуты\n"
        "12 - Информация о файле + время\n"
        "13 - Изменить время файла\n"
        "0  - Выход\n\n";
}

int main() {


    setlocale(LC_ALL, "");

    while (true) {

        printHeader("Win32 Управление файловой системой");
        showMenu();

        int choice = getIntInput(0, 13);

        if (choice == 0)
            break;

        switch (choice) {
            case 1:
                printHeader("Список дисков (GetLogicalDrives)");
                listDisks_GetLogicalDrives();
                break;

            case 2:
                printHeader("Список дисков (GetLogicalDriveStrings)");
                listDisks_GetLogicalDriveStrings();
                break;

            case 3:
                printHeader("Информация о диске");
                diskInfoAndFreeSpace();
                break;

            case 4:
                printHeader("Создание каталога");
                createDirectoryMenu();
                break;

            case 5:
                printHeader("Удаление каталога");
                removeDirectoryMenu();
                break;

            case 6:
                printHeader("Создание файла");
                createFileInDirectory();
                break;

            case 7:
                printHeader("Копирование файла");
                copyFileMenu();
                break;

            case 8:
                printHeader("Перемещение файла");
                moveFileMenu();
                break;

            case 9:
                printHeader("MoveFileEx");
                moveFileExMenu();
                break;

            case 10:
                printHeader("Просмотр атрибутов");
                showAttributes();
                break;

            case 11:
                printHeader("Изменение атрибутов");
                toggleAttributes();
                break;

            case 12:
                printHeader("Информация о файле");
                fileInfoByHandleAndTimes();
                break;

            case 13:
                printHeader("Изменение времени файла");
                setFileTimeMenu();
                break;
        }

        pauseScreen();
    }

    return 0;
}

