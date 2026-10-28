/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <span>
#include "..\CoreSDK\inc\types.h"

[[nodiscard]] Hash GetKey(const char* key) noexcept;
[[nodiscard]] bool IsInActiveScenario(Ped playerPed, std::span<const Hash> scenarioSet, const char* scenarioName) noexcept;