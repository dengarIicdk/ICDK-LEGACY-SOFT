#include <thread>
#include <iostream>
#include <array>
#include "memory.h"

using namespace std;

int garb1(int x)
{
	int y = x;
	y = 10 - 10;
	x = y - 293;
	return x;
}

float randomFloat() {
	return (float)rand() / RAND_MAX;
}

int stringToInt(std::string str) {
	int result = 0;
	for (int i = 0; i < str.length(); i++) {
		result += (int)str[i];
	}
	return result;
}

int** addMatrices(int** matrix1, int** matrix2, int rows, int cols) {
	int** result = new int* [rows];
	for (int i = 0; i < rows; i++) {
		result[i] = new int[cols];
		for (int j = 0; j < cols; j++) {
			result[i][j] = matrix1[i][j] + matrix2[i][j];
		}
	}
	return result;
}

int factorial(int n) {
	if (n <= 1) {
		return 1;
	}
	return n * factorial(n - 1);
}

namespace offset
{

	constexpr ::std::ptrdiff_t dwLocalPlayerController = 0x22F5028;
	constexpr ::std::ptrdiff_t dwLocalPlayerPawn = 0x206A9E0;
	constexpr ::std::ptrdiff_t dwEntityList = 0x24B0258;
	constexpr ::std::ptrdiff_t dwNetworkGameClient = 0x9095D0;

	constexpr ::std::ptrdiff_t m_hMyWeapons = 0x48;

	constexpr ::std::ptrdiff_t m_flFallbackWear = 0x1858;
	constexpr ::std::ptrdiff_t m_nFallbackPaintKit = 0x1850;
	constexpr ::std::ptrdiff_t m_nFallbackSeed = 0x1854;
	constexpr ::std::ptrdiff_t m_nFallbackStatTrak = 0x185C;
	constexpr ::std::ptrdiff_t m_iItemDefinitionIndex = 0x1BA;
	constexpr ::std::ptrdiff_t m_iItemIDHigh = 0x1D0;
	constexpr ::std::ptrdiff_t m_iEntityQuality = 0x1BC;
	constexpr ::std::ptrdiff_t m_iAccountID = 0x1D8;
	constexpr ::std::ptrdiff_t m_OriginalOwnerXuidLow = 0x1848;
	constexpr ::std::ptrdiff_t m_pWeaponServices = 0x13D8;
	constexpr ::std::ptrdiff_t m_pViewModelServices = 0x1368;
	constexpr std::ptrdiff_t m_hViewModel = 0x40;
}

constexpr const int GetWeaponPaint(const short& itemdefinition)
{

	switch (itemdefinition)
	{

	case 7: return 433;
	case 9: return 344;

	case 4: return 437;
	case 61:return 504;
	default:return 0;
	}

}

int main()
{
	cout << "Press enter to start" << endl;
	cin.get();

	cout << "[DEBUG] Initializing memory..." << endl;
	const auto memory = Memory("cs2.exe");

	cout << "[DEBUG] Fetching module addresses..." << endl;
	const auto client = memory.GetModuleAddress("client.dll");
	const auto engine = memory.GetModuleAddress("engine2.dll");

	if (!client || !engine)
	{
		cout << "[ERROR] Failed to get module address." << endl;
		return 0;
	}

	cout << "[DEBUG] Client base: 0x" << hex << client << endl;
	cout << "[DEBUG] Engine base: 0x" << hex << engine << endl;

	while (true)
	{
		// 1. Получаем контроллер и павн (исправлено)
		const auto localPlayerController = memory.Read<uintptr_t>(client + offset::dwLocalPlayerController);
		const auto localPlayerPawn = memory.Read<uintptr_t>(client + offset::dwLocalPlayerPawn);

		if (!localPlayerController || !localPlayerPawn) continue;

		// 2. Заходим в сервисы оружия
		const auto m_pWeaponServices = memory.Read<uintptr_t>(localPlayerPawn + offset::m_pWeaponServices);
		if (!m_pWeaponServices) continue;

		// 3. Читаем количество оружия (Size вектора)
		const auto weaponCount = memory.Read<int>(m_pWeaponServices + offset::m_hMyWeapons + 0x8);

		bool updateNeeded = false;

		// 4. Цикл по оружию (ограничим 8 слотами для безопасности)
		for (int i = 0; i < (weaponCount > 8 ? 8 : weaponCount); i++)
		{
			// Читаем хендл из массива
			uint32_t handle = memory.Read<uint32_t>(m_pWeaponServices + offset::m_hMyWeapons + (i * 0x4));
			if (!handle || handle == 0xFFFFFFFF) continue;

			// Получаем сущность оружия через EntityList
			const auto listEntry = memory.Read<uintptr_t>(client + offset::dwEntityList + (0x8 * ((handle & 0x7FFF) >> 9)) + 16);
			if (!listEntry) continue;

			const auto weapon = memory.Read<uintptr_t>(listEntry + 120 * (handle & 0x1FF));
			if (!weapon) continue;

			// Проверяем индекс оружия
			const auto itemDefIndex = memory.Read<short>(weapon + offset::m_iItemDefinitionIndex);
			const auto paint = GetWeaponPaint(itemDefIndex);

			if (paint > 0)
			{
				// Проверяем, нужно ли записывать (чтобы не писать каждый кадр)
				if (memory.Read<int32_t>(weapon + offset::m_nFallbackPaintKit) != paint)
				{
					// Записываем данные скина
					memory.Write<int32_t>(weapon + offset::m_iItemIDHigh, -1);
					memory.Write<int32_t>(weapon + offset::m_nFallbackPaintKit, paint);
					memory.Write<float>(weapon + offset::m_flFallbackWear, 0.001f); // Factory New
					memory.Write<int32_t>(weapon + offset::m_iAccountID, memory.Read<int32_t>(weapon + offset::m_OriginalOwnerXuidLow));

					updateNeeded = true;
					cout << "[DEBUG] Skin " << paint << " applied to weapon " << itemDefIndex << endl;
				}
			}
		}

		// 5. Принудительное обновление (только если что-то изменили)
		if (updateNeeded)
		{
			uintptr_t ncw = memory.Read<uintptr_t>(engine + offset::dwNetworkGameClient);
			if (ncw) {
				memory.Write<int32_t>(ncw + 0x258, -1); // Force Full Update
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(200)); // Небольшая пауза
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Снижаем нагрузку на CPU
	}

		const auto weaponCount = memory.Read<int>(client + offset::m_hMyWeapons + 0x8);
		cout << "[DEBUG] Weapon Count: " << weaponCount << endl;

		array<uintptr_t, 8> weapons{};
		for (int i = 0; i < weaponCount; i++)
		{
			weapons[i] = memory.Read<uintptr_t>(client + offset::m_hMyWeapons + (i * 0x4));
		}

		bool update = false;
		for (const auto& handle : weapons)
		{
			if (!handle)
				continue;

			const auto weaponListEntry = memory.Read<uintptr_t>(
				client + offset::dwEntityList + (0x8 * ((handle & 0x7FFF) >> 9)) + 16);
			if (!weaponListEntry)
			{
				cout << "[WARNING] Invalid weapon list entry." << endl;
				continue;
			}

			const auto weapon = memory.Read<uintptr_t>(weaponListEntry + 120 * (handle & 0x1FF));
			if (!weapon)
			{
				cout << "[WARNING] Invalid weapon entity." << endl;
				continue;
			}

			cout << "[DEBUG] Weapon Entity: 0x" << hex << weapon << endl;

			const auto itemDefIndex = memory.Read<short>(weapon + offset::m_iItemDefinitionIndex);
			cout << "[DEBUG] Weapon Item Definition Index: " << dec << itemDefIndex << endl;

			if (const auto paint = GetWeaponPaint(itemDefIndex))
			{
				cout << "[DEBUG] Applying skin: " << paint << endl;

				bool check = memory.Read<int32_t>(weapon + offset::m_nFallbackPaintKit) != paint;
				if (check)
				{
					update = true;
				}

				int32_t currentPaint = memory.Read<int32_t>(weapon + offset::m_nFallbackPaintKit);
				if (currentPaint != paint) {
					// Только если скин не совпадает, пишем новые данные
					memory.Write<int32_t>(weapon + 0x1D0, -1); // m_iItemIDHigh
					memory.Write<int32_t>(weapon + 0x1D4, -1); // m_iItemIDLow (часто идет сразу за High)
					memory.Write<int32_t>(weapon + offset::m_iItemIDHigh, -1);
					memory.Write<int32_t>(weapon + offset::m_nFallbackPaintKit, paint);
					memory.Write<float>(weapon + offset::m_flFallbackWear, 0.1f);
					memory.Write<int32_t>(weapon + offset::m_nFallbackStatTrak, 6969);
					memory.Write<int32_t>(weapon + offset::m_iAccountID, memory.Read<int32_t>(weapon + offset::m_OriginalOwnerXuidLow));

					cout << "[DEBUG] Skin applied successfully!" << endl;
				}
			}

			const auto m_pViewModelServices = memory.Read<uintptr_t>(client + offset::m_pViewModelServices);
			if (m_pViewModelServices)
			{
				const auto ViewHandle = memory.Read<uintptr_t>(m_pViewModelServices + offset::m_hViewModel);
				if (ViewHandle)
				{
					const auto ViewListEntry = memory.Read<uintptr_t>(
						client + offset::dwEntityList + (0x8 * ((ViewHandle & 0x7FFF) >> 9)) + 16);
					if (ViewListEntry)
					{
						const auto ViewController = memory.Read<uintptr_t>(ViewListEntry + 120 * (ViewHandle & 0x1FF));
						if (ViewController)
						{
							const auto ViewNode = memory.Read<uintptr_t>(ViewController + 0x318);
							const auto ViewMask = memory.Read<uint64_t>(ViewNode + 0x160);
							if (ViewMask != 2)
							{
								memory.Write<uint64_t>(ViewNode + 0x160, 2);
								cout << "[DEBUG] Forced ViewModel update." << endl;
							}
						}
					}
				}
			}

			// Обновленный блок Force Update
			if (update)
			{
				uintptr_t ncw = memory.Read<uintptr_t>(engine + offset::dwNetworkGameClient);
				if (ncw) {
					// Установка дельта-тика в -1 заставляет клиент перекачать состояние сущностей
					memory.Write<int32_t>(ncw + 0x258, -1);
				}
				update = false;
			}
		}

		return 0;
	}
