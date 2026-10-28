/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <mutex>
#include <array>
#include "..\CoreSDK\inc\types.h"

class PlayerTracker {
    int m_PlayerSamples = 0;

	// movement tracking for player on foot
    float m_PlayerFootSum = 0.0f;
    int m_PlayerFootSamples = 0;

	// movement tracking for horse when player is mounted
    float m_HorseSum = 0.0f;
    int m_HorseSamples = 0;

	float m_PlayerAccumulatedDirt = 0.0f;
	int m_PlayerCampSamples = 0;

    std::array<Hash, 33> m_CampScenarios{};

    std::mutex m_Lock;

public:
    void ComputeHashes() noexcept;
    void RecordSnapshot(Ped playerPed) noexcept;

    [[nodiscard]] float GetPlayerOnFootSpeed() noexcept;
    [[nodiscard]] float GetHorseAverageSpeed() noexcept;
    [[nodiscard]] bool IsPlayerDirty(Ped playerPed) noexcept;
    [[nodiscard]] bool IsPlayerAtCamp(Ped playerPed, bool rightNow) noexcept;
    void ResetSnapshot() noexcept;
};