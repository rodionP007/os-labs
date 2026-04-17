// PipeServer.cpp
// cl /EHsc /std:c++17 PipeServer.cpp

#include <windows.h>
#include <iostream>
#include <string>

static const wchar_t* PIPE_NAME = L"\\\\.\\pipe\\LR4DemoPipe";

struct ServerCtx {
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE hEvent = NULL;
    OVERLAPPED ov{};
    bool connected = false;
};

static bool createPipe(ServerCtx& ctx) {
    if (ctx.hPipe != INVALID_HANDLE_VALUE) {
        std::cout << "Pipe already created\n";
        return true;
    }

    ctx.hPipe = CreateNamedPipeW(
        PIPE_NAME,
        PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        4096,
        4096,
        0,
        nullptr
    );

    if (ctx.hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "CreateNamedPipe failed: " << GetLastError() << "\n";
        return false;
    }

    ctx.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!ctx.hEvent) {
        std::cout << "CreateEvent failed: " << GetLastError() << "\n";
        CloseHandle(ctx.hPipe);
        ctx.hPipe = INVALID_HANDLE_VALUE;
        return false;
    }

    ZeroMemory(&ctx.ov, sizeof(ctx.ov));
    ctx.ov.hEvent = ctx.hEvent;

    std::cout << "Pipe created\n";
    return true;
}

static bool connectClient(ServerCtx& ctx) {
    if (ctx.hPipe == INVALID_HANDLE_VALUE) {
        std::cout << "Create pipe first\n";
        return false;
    }
    if (ctx.connected) {
        std::cout << "Client already connected\n";
        return true;
    }

    ResetEvent(ctx.hEvent);

    BOOL ok = ConnectNamedPipe(ctx.hPipe, &ctx.ov);
    DWORD err = GetLastError();

    if (!ok) {
        if (err == ERROR_IO_PENDING) {
            std::cout << "Waiting for client...\n";
            DWORD wr = WaitForSingleObject(ctx.hEvent, INFINITE);
            if (wr != WAIT_OBJECT_0) {
                std::cout << "Wait failed\n";
                return false;
            }
        } else if (err == ERROR_PIPE_CONNECTED) {
            SetEvent(ctx.hEvent);
        } else {
            std::cout << "ConnectNamedPipe failed: " << err << "\n";
            return false;
        }
    }

    ctx.connected = true;
    std::cout << "Client connected\n";
    return true;
}

static bool writeAsync(ServerCtx& ctx, const std::string& msg) {
    if (ctx.hPipe == INVALID_HANDLE_VALUE || !ctx.connected) {
        std::cout << "No client connected\n";
        return false;
    }

    ResetEvent(ctx.hEvent);

    DWORD bytesWritten = 0;
    BOOL ok = WriteFile(
        ctx.hPipe,
        msg.c_str(),
        (DWORD)msg.size() + 1,
        nullptr,
        &ctx.ov
    );

    if (!ok) {
        DWORD err = GetLastError();
        if (err != ERROR_IO_PENDING) {
            std::cout << "WriteFile failed: " << err << "\n";
            return false;
        }
    }

    DWORD wr = WaitForSingleObject(ctx.hEvent, INFINITE);
    if (wr != WAIT_OBJECT_0) {
        std::cout << "WaitForSingleObject failed\n";
        return false;
    }

    if (!GetOverlappedResult(ctx.hPipe, &ctx.ov, &bytesWritten, FALSE)) {
        std::cout << "GetOverlappedResult failed: " << GetLastError() << "\n";
        return false;
    }

    std::cout << "Sent " << bytesWritten << " bytes\n";
    return true;
}

static void disconnectClient(ServerCtx& ctx) {
    if (ctx.hPipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(ctx.hPipe);
        ctx.connected = false;
        std::cout << "Client disconnected\n";
    }
}

static void closeAll(ServerCtx& ctx) {
    if (ctx.hPipe != INVALID_HANDLE_VALUE) {
        CloseHandle(ctx.hPipe);
        ctx.hPipe = INVALID_HANDLE_VALUE;
    }
    if (ctx.hEvent) {
        CloseHandle(ctx.hEvent);
        ctx.hEvent = NULL;
    }
}

int main() {
    ServerCtx ctx;

    while (true) {
        std::cout << "\n--- PIPE SERVER ---\n";
        std::cout << "1. CreateNamedPipe\n";
        std::cout << "2. ConnectNamedPipe\n";
        std::cout << "3. WriteFile (async)\n";
        std::cout << "4. DisconnectNamedPipe\n";
        std::cout << "0. Exit\n";
        std::cout << "Choose: ";

        int cmd;
        std::cin >> cmd;
        std::cin.ignore();

        if (cmd == 0) break;

        switch (cmd) {
        case 1:
            createPipe(ctx);
            break;
        case 2:
            connectClient(ctx);
            break;
        case 3: {
            std::string msg;
            std::cout << "Enter message: ";
            std::getline(std::cin, msg);
            writeAsync(ctx, msg);
            break;
        }
        case 4:
            disconnectClient(ctx);
            break;
        default:
            std::cout << "Unknown command\n";
            break;
        }
    }

    closeAll(ctx);
    return 0;
}
