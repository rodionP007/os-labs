// PipeClient.cpp
// cl /EHsc /std:c++17 PipeClient.cpp

#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

static const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\LR4DemoPipe";

struct ClientCtx {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    OVERLAPPED ov{};
    std::vector<char> buffer;
};

VOID WINAPI ReadCompleted(DWORD errorCode, DWORD bytesTransferred, LPOVERLAPPED lpOverlapped) {
    ClientCtx* ctx = reinterpret_cast<ClientCtx*>(lpOverlapped->hEvent);

    if (errorCode != 0) {
        std::cout << "Read completion error: " << errorCode << "\n";
        return;
    }

    if (bytesTransferred == 0) {
        std::cout << "No data received\n";
        return;
    }

    std::cout << "Received: " << ctx->buffer.data() << "\n";
}

static bool connectPipe(ClientCtx& ctx) {
    if (ctx.hPipe != INVALID_HANDLE_VALUE) {
        std::cout << "Already connected\n";
        return true;
    }

    ctx.hPipe = CreateFileW(
        PIPE_NAME,
        GENERIC_READ,
        0,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OVERLAPPED,
        nullptr
    );

    if (ctx.hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "CreateFile failed: " << GetLastError() << "\n";
        return false;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    SetNamedPipeHandleState(ctx.hPipe, &mode, nullptr, nullptr);

    ctx.buffer.resize(4096);
    ZeroMemory(&ctx.ov, sizeof(ctx.ov));

    // Хитрость: передаем указатель на ClientCtx через hEvent, чтобы достать его в callback
    ctx.ov.hEvent = reinterpret_cast<HANDLE>(&ctx);

    std::cout << "Connected to pipe\n";
    return true;
}

static bool readAsync(ClientCtx& ctx) {
    if (ctx.hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "Connect first\n";
        return false;
    }

    ZeroMemory(ctx.buffer.data(), ctx.buffer.size());

    BOOL ok = ReadFileEx(
        ctx.hPipe,
        ctx.buffer.data(),
        (DWORD)ctx.buffer.size(),
        &ctx.ov,
        ReadCompleted
    );

    if (!ok) {
        std::cout << "ReadFileEx failed: " << GetLastError() << "\n";
        return false;
    }

    std::cout << "Waiting for async read completion...\n";
    SleepEx(INFINITE, TRUE); // alertable wait
    return true;
}

static void disconnectPipe(ClientCtx& ctx) {
    if (ctx.hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx.hPipe);
        ctx.hPipe = INVALID_HANDLE_VALUE;
        std::cout << "Disconnected\n";
    }
}

int main() {
    ClientCtx ctx;

    while (true) {
        std::cout << "\n--- PIPE CLIENT ---\n";
        std::cout << "1. CreateFile (connect)\n";
        std::cout << "2. ReadFileEx\n";
        std::cout << "3. Disconnect\n";
        std::cout << "0. Exit\n";
        std::cout << "Choose: ";

        int cmd;
        std::cin >> cmd;
        std::cin.ignore();

        if (cmd == 0) break;

        switch (cmd) {
        case 1:
            connectPipe(ctx);
            break;
        case 2:
            readAsync(ctx);
            break;
        case 3:
            disconnectPipe(ctx);
            break;
        default:
            std::cout << "Unknown command\n";
            break;
        }
    }

    disconnectPipe(ctx);
    return 0;
}
