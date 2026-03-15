#pragma once
#include <windows.h>
#include <cstdint>
#pragma comment(lib, "user32.lib")

// ПРЯМЫЕ ПУТИ К ДАМПЕРУ
#include "Z:/icdk_e/dumper/output/client_dll.hpp"
#include "Z:/icdk_e/dumper/output/offsets.hpp"

namespace memory {
    template <typename T>
    static T Read(uintptr_t address) {
        if (!address) return T();
        return *reinterpret_cast<T*>(address);
    }

    template <typename T>
    static void Write(uintptr_t address, T value) {
        if (!address) return;
        *reinterpret_cast<T*>(address) = value;
    }
}
