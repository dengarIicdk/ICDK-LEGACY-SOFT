#include <iostream>
#include <windows.h>
#include <string>
#include <thread>
#include <fstream>
#include <wininet.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "wininet.lib")

const char* DLL_PATH = "Z:\\icdk_e\\icdk_internal_final\\source\\ICDK_Internal.dll";
const char* CONFIG_FILE = "Z:\\icdk_e\\icdk_internal_final\\skin_config.txt";

// Функция для записи конфига (теперь вызывается сайтом)
void WriteConfig(std::string data) {
    std::ofstream file(CONFIG_FILE, std::ios::trunc);
    if (file.is_open()) {
        file << data;
        file.close();
        std::cout << "[SERVER] Config updated: " << data << std::endl;
    }
}

// В реальном проекте тут был бы HTTP сервер, но для скорости 
// мы сделаем проверку буфера обмена (Сайт копирует - Лоадер видит и пишет в файл)
void ClipboardWatcher() {
    std::string lastContent = "";
    while (true) {
        if (OpenClipboard(NULL)) {
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* pszText = static_cast<char*>(GlobalLock(hData));
                if (pszText) {
                    std::string currentContent(pszText);
                    // Если в буфере строка формата "ID:0x..."
                    if (currentContent != lastContent && currentContent.find(':') != std::string::npos) {
                        WriteConfig(currentContent);
                        lastContent = currentContent;
                    }
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
        }
        Sleep(500);
    }
}

void Inject(DWORD procId) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, 0, procId);
    void* loc = VirtualAllocEx(hProc, 0, MAX_PATH, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, loc, DLL_PATH, strlen(DLL_PATH) + 1, 0);
    HANDLE hThread = CreateRemoteThread(hProc, 0, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, loc, 0, 0);
    if (hThread) CloseHandle(hThread);
    CloseHandle(hProc);
}

int main() {
    std::cout << "[ICDK-SERVER] Bridge Online. Waiting for Site Commands..." << std::endl;

    // Запускаем слежку за командами от сайта в отдельном потоке
    std::thread(ClipboardWatcher).detach();

    HWND window = NULL;
    while (!window) {
        window = FindWindowA("SDL_app", "Counter-Strike 2");
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    DWORD procId;
    GetWindowThreadProcessId(window, &procId);
    Inject(procId);

    std::cout << "[!] System Operational. Don't close this window!" << std::endl;
    while (true) Sleep(1000); // Держим лоадер открытым как сервер
    return 0;
}
