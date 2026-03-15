/**
 * ICDK-730-LEGACY // INTERNAL_CORE_PROJECT
 * VERSION: 10.5 [EXTENDED_HEAVY_SDK]
 * ENTITY_LIST_FIX: 0x24AC2A8
 */

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "Psapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

#include <windows.h>
#include <iostream>
#include <thread>
#include <fstream>
#include <string>
#include <vector>
#include <psapi.h>
#include <mutex>

 // ПОДКЛЮЧАЕМ MINHOOK
#include "minhook/MinHook.h" 
#pragma comment(lib, "minhook/libMinHook.x64.lib")

#include "memory.h"

using namespace cs2_dumper;

// --- ГЛОБАЛЬНЫЕ ПАРАМЕТРЫ И БУФЕРЫ ---
const uintptr_t ENTITY_LIST_OFFSET = 0x24AC2A8; // ТВОЙ ПРОВЕРЕННЫЙ ОФФСЕТ
const char* CONFIG_FILE_PATH = "Z:\\icdk_e\\icdk_internal_final\\skin_config.txt";

typedef void(__fastcall* FrameStageNotify_t)(void*, int);
FrameStageNotify_t oFrameStageNotify = nullptr;

// Глобальное хранилище данных (Связь с сайтом)
struct SharedConfig {
    int targetKnife = 0;
    int targetSkin = 0;
    bool needsUpdate = false;
} g_Sync;

std::mutex mtx_sync; // Защита от краша при доступе из разных потоков

// --- СИСТЕМА ДИАГНОСТИКИ И ВАЛИДАЦИИ ---

// Проверка: живой ли адрес памяти (Anti-Crash)
bool IsValidMemory(uintptr_t address) {
    if (address == 0) return false;
    if (address < 0x100000) return false;
    if (address > 0x7FFFFFFFFFFF) return false;
    return true;
}

// Поиск интерфейса в модуле (SDK Style)
void* GetInterface(const char* moduleName, const char* interfaceName) {
    HMODULE hMod = GetModuleHandleA(moduleName);
    if (!hMod) return nullptr;

    auto CreateInterfaceFn = (void* (*)(const char*, int*))GetProcAddress(hMod, "CreateInterface");
    if (!CreateInterfaceFn) return nullptr;

    void* result = CreateInterfaceFn(interfaceName, nullptr);
    if (result) {
        std::cout << "[SDK] Captured Interface: " << interfaceName << " at " << result << std::endl;
    }
    return result;
}

// Фоновый поток для мониторинга файла skin_config.txt
void GlobalConfigWatcher() {
    std::cout << "[SYSTEM] Config Watcher Thread Online." << std::endl;
    while (true) {
        std::ifstream file(CONFIG_FILE_PATH);
        if (file.is_open()) {
            std::string line;
            if (std::getline(file, line) && line.find(':') != std::string::npos) {
                size_t separator = line.find(':');
                std::lock_guard<std::mutex> lock(mtx_sync);
                try {
                    g_Sync.targetKnife = std::stoi(line.substr(0, separator));
                    g_Sync.targetSkin = std::stoul(line.substr(separator + 1), nullptr, 16);
                }
                catch (...) { /* Ошибка формата - пропускаем */ }
            }
            file.close();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
}

// --- ХУК FrameStageNotify (ТОЧКА ВХОДА В ДВИЖОК) ---
void __fastcall hkFrameStageNotify(void* rcx, int stage) {
    // Используем SEH-броню для предотвращения вылета при загрузке карты
    __try {
        // Stage 5 = FRAME_NET_UPDATE_POSTDATAUPDATE_START
        if (stage == 5) {
            uintptr_t client = (uintptr_t)GetModuleHandleA("client.dll");
            if (!IsValidMemory(client)) goto CALL_ORIGIN;

            // 1. Получаем Pawn (Тело игрока) через оффсет из дампера
            uintptr_t localPawn = memory::Read<uintptr_t>(client + offsets::client_dll::dwLocalPlayerPawn);
            if (!IsValidMemory(localPawn)) goto CALL_ORIGIN;

            // Извлекаем данные из синхронизированного кэша
            int kId = 0, sId = 0;
            {
                std::lock_guard<std::mutex> lock(mtx_sync);
                kId = g_Sync.targetKnife;
                sId = g_Sync.targetSkin;
            }

            if (kId > 0) {
                // 2. Идем в сервис оружия
                uintptr_t weaponServices = memory::Read<uintptr_t>(localPawn + schemas::client_dll::C_BasePlayerPawn::m_pWeaponServices);
                if (!IsValidMemory(weaponServices)) goto CALL_ORIGIN;

                // 3. Получаем хэндл активного оружия
                uint32_t hActiveWeapon = memory::Read<uint32_t>(weaponServices + schemas::client_dll::CPlayer_WeaponServices::m_hActiveWeapon);
                if (hActiveWeapon == 0xFFFFFFFF) goto CALL_ORIGIN;

                // 4. ИСПОЛЬЗУЕМ ТВОЙ dwEntityList (0x24AC2A8)
                uintptr_t entityList = memory::Read<uintptr_t>(client + ENTITY_LIST_OFFSET);
                if (!IsValidMemory(entityList)) goto CALL_ORIGIN;

                // Математика доступа к сущности (Entity)
                uintptr_t listEntry = memory::Read<uintptr_t>(entityList + 0x8 * ((hActiveWeapon & 0x7FFF) >> 9) + 16);
                if (!IsValidMemory(listEntry)) goto CALL_ORIGIN;

                uintptr_t weaponEntity = memory::Read<uintptr_t>(listEntry + 120 * (hActiveWeapon & 0x1FF));
                if (IsValidMemory(weaponEntity)) {

                    // ПРИНУДИТЕЛЬНАЯ ЗАПИСЬ (SURGERY)
                    memory::Write<int>(weaponEntity + schemas::client_dll::C_EconItemView::m_iItemDefinitionIndex, kId);
                    memory::Write<int>(weaponEntity + schemas::client_dll::C_EconEntity::m_nFallbackPaintKit, sId);
                    memory::Write<int>(weaponEntity + schemas::client_dll::C_EconItemView::m_iItemIDHigh, -1);
                    memory::Write<float>(weaponEntity + schemas::client_dll::C_EconEntity::m_flFallbackWear, 0.0001f);

                    // СИЛОВОЙ ПИНОК (Мгновенное обновление модели)
                    int currentLow = memory::Read<int>(weaponEntity + schemas::client_dll::C_EconItemView::m_iItemIDLow);
                    memory::Write<int>(weaponEntity + schemas::client_dll::C_EconItemView::m_iItemIDLow, currentLow + 1);
                }
            }
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        // Если память "битая" - просто выходим из хука без краша
    }

CALL_ORIGIN:
    oFrameStageNotify(rcx, stage);
}

// --- ОСНОВНОЙ ПОТОК (INITIALIZATION) ---
void MainThread(HMODULE hModule) {
    AllocConsole();
    FILE* f; freopen_s(&f, "CONOUT$", "w", stdout);

    std::cout << "========================================" << std::endl;
    std::cout << "   ICDK_INTERNAL // HEAVY_SDK v10.5     " << std::endl;
    std::cout << "   ENTITY_LIST: 0x24AC2A8               " << std::endl;
    std::cout << "========================================" << std::endl;

    // 1. Запуск потока конфига
    std::thread(GlobalConfigWatcher).detach();

    // 2. Инициализация MinHook
    if (MH_Initialize() != MH_OK) {
        std::cout << "[-] FATAL: MinHook failed to init!" << std::endl;
        return;
    }

    // 3. Получение интерфейса Source2Client
    void* clientInt = GetInterface("client.dll", "Source2Client002");

    if (clientInt) {
        void** vmt = *(void***)clientInt;
        void* fsn_target = vmt; // FrameStageNotify индекс 35 в VMT

        std::cout << "[+] Hooking FrameStageNotify at: " << fsn_target << std::endl;

        if (MH_CreateHook(fsn_target, &hkFrameStageNotify, (LPVOID*)&oFrameStageNotify) == MH_OK) {
            MH_EnableHook(MH_ALL_HOOKS);
            std::cout << "[!] HOOK ESTABLISHED. Surgery Module Online." << std::endl;
        }
    }
    else {
        std::cout << "[-] ERROR: Source2Client Interface not found!" << std::endl;
    }

    // Цикл жизни чита
    while (!GetAsyncKeyState(VK_END)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    // Безопасная выгрузка
    std::cout << "[SYSTEM] Unhooking and releasing resources..." << std::endl;
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (f) fclose(f);
    FreeConsole();
    FreeLibraryAndExitThread(hModule, 0);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE hThread = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)MainThread, hModule, 0, nullptr);
        if (hThread) CloseHandle(hThread);
    }
    return TRUE;
}
