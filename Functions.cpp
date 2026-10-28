/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <span>
#include <algorithm>
#include "..\CoreSDK\inc\types.h"
#include "..\CoreSDK\inc\natives.h"
#include "Functions.h"
#include "Logger.h"

[[nodiscard]] Hash GetKey(const char* key) noexcept {
    Hash hash = MISC::GET_HASH_KEY(key);
    LOG_DEV("Precomputed hash for key '{}': {}", key, hash);

    return hash;
}

[[nodiscard]] bool IsInActiveScenario(Ped playerPed, std::span<const Hash> scenarioSet, const char* scenarioName) noexcept {
    Hash activeScenario = TASK::_GET_SCENARIO_POINT_TYPE_PED_IS_USING(playerPed);
    LOG_DEV("Active scenario hash: {}", activeScenario);

    // Fall back to transient dynamic actions if the primary structural anchor returns empty
    if (activeScenario == 0) {
        activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);
        LOG_DEV("Fallback to dynamic scenario hash: {}", activeScenario);
    }

    if (activeScenario == 0) return false;

    auto contains_check = [activeScenario](Hash scenarioHash) {
        return scenarioHash == activeScenario;
        };

    bool isInScenarioSet = std::ranges::any_of(scenarioSet, contains_check);

    LOG_DEV("Is active scenario ({}) in known set: {}", scenarioName, isInScenarioSet ? "true" : "false");

    return isInScenarioSet;
}