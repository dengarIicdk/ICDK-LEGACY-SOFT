#include <iostream>
#include <windows.h>
#include <tlhelp32.h>
#include <fstream>
#include <vector>

struct MappingData { void* pLoadLibraryA; void* pGetProcAddress; void* pBase; };

DWORD GetProcId(const char* procName) {
    DWORD procId = 0;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe; pe.dwSize = sizeof(pe);
        if (Process32First(hSnap, &pe)) {
            do { if (!_stricmp(pe.szExeFile, procName)) { procId = pe.th32ProcessID; break; } } while (Process32Next(hSnap, &pe));
        }
    }
    CloseHandle(hSnap); return procId;
}

// Shell-код для Manual Map
void __stdcall LibraryLoader(MappingData* pData) {
    if (!pData) return;
    auto pBase = (unsigned char*)pData->pBase;
    auto pOpt = &((IMAGE_NT_HEADERS*)(pBase + ((IMAGE_DOS_HEADER*)pBase)->e_lfanew))->OptionalHeader;
    auto _LoadLibraryA = (decltype(&LoadLibraryA))pData->pLoadLibraryA;
    auto _GetProcAddress = (decltype(&GetProcAddress))pData->pGetProcAddress;

    auto pReloc = (IMAGE_BASE_RELOCATION*)(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress);
    unsigned __int64 delta = (unsigned __int64)pBase - pOpt->ImageBase;
    while (pReloc->VirtualAddress) {
        unsigned int count = (pReloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(unsigned short);
        unsigned short* pInfo = (unsigned short*)(pReloc + 1);
        for (unsigned int i = 0; i < count; ++i) if ((pInfo[i] >> 12) == IMAGE_REL_BASED_DIR64) *(unsigned __int64*)(pBase + pReloc->VirtualAddress + (pInfo[i] & 0xFFF)) += delta;
        pReloc = (IMAGE_BASE_RELOCATION*)((unsigned char*)pReloc + pReloc->SizeOfBlock);
    }

    auto pImport = (IMAGE_IMPORT_DESCRIPTOR*)(pBase + pOpt->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    while (pImport->Name) {
        auto hMod = _LoadLibraryA((char*)(pBase + pImport->Name));
        auto pThunk = (IMAGE_THUNK_DATA64*)(pBase + pImport->FirstThunk);
        auto pOrig = (IMAGE_THUNK_DATA64*)(pBase + pImport->OriginalFirstThunk);
        while (pOrig->u1.AddressOfData) {
            pThunk->u1.Function = (unsigned __int64)_GetProcAddress(hMod, IMAGE_SNAP_BY_ORDINAL64(pOrig->u1.Ordinal) ? (char*)(pOrig->u1.Ordinal & 0xFFFF) : ((IMAGE_IMPORT_BY_NAME*)(pBase + pOrig->u1.AddressOfData))->Name);
            pThunk++; pOrig++;
        }
        pImport++;
    }
    ((BOOL(APIENTRY*)(HMODULE, DWORD, LPVOID))(pBase + pOpt->AddressOfEntryPoint))((HMODULE)pBase, DLL_PROCESS_ATTACH, nullptr);
}
void __stdcall Stub() {}

void BridgeUpdate() {
    if (!OpenClipboard(nullptr)) return;
    HANDLE hData = GetClipboardData(CF_TEXT);
    if (hData) {
        if (char* text = (char*)GlobalLock(hData)) {
            if (strstr(text, "0x")) {
                std::ofstream f("config.json");
                if (f.is_open()) { f << text; f.close(); std::cout << "[BRIDGE] Updated config: " << text << std::endl; }
                EmptyClipboard();
            }
            GlobalUnlock(hData);
        }
    }
    CloseClipboard();
}

int main() {
    const char* dll = "ICDK_Core.dll";
    DWORD pid = 0;
    while (!(pid = GetProcId("cs2.exe"))) Sleep(1000);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    std::ifstream f(dll, std::ios::binary | std::ios::ate);
    size_t size = f.tellg(); std::vector<char> buf(size); f.seekg(0); f.read(buf.data(), size); f.close();

    auto pNt = (IMAGE_NT_HEADERS64*)(buf.data() + ((IMAGE_DOS_HEADER*)buf.data())->e_lfanew);
    void* pBase = VirtualAllocEx(hProc, nullptr, pNt->OptionalHeader.SizeOfImage, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(hProc, pBase, buf.data(), pNt->OptionalHeader.SizeOfHeaders, nullptr);
    auto pSec = IMAGE_FIRST_SECTION(pNt);
    for (int i = 0; i < pNt->FileHeader.NumberOfSections; i++) WriteProcessMemory(hProc, (void*)((unsigned __int64)pBase + pSec[i].VirtualAddress), buf.data() + pSec[i].PointerToRawData, pSec[i].SizeOfRawData, nullptr);

    MappingData d = { LoadLibraryA, GetProcAddress, pBase };
    void* pd = VirtualAllocEx(hProc, nullptr, sizeof(d), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    WriteProcessMemory(hProc, pd, &d, sizeof(d), nullptr);
    void* ps = VirtualAllocEx(hProc, nullptr, 1024, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    WriteProcessMemory(hProc, ps, LibraryLoader, 1024, nullptr);

    CreateRemoteThread(hProc, nullptr, 0, (LPTHREAD_START_ROUTINE)ps, pd, 0, nullptr);
    std::cout << "[!] STEALTH INJECTED. Bridge Active." << std::endl;
    while (GetProcId("cs2.exe")) { BridgeUpdate(); Sleep(500); }
    return 0;
}
