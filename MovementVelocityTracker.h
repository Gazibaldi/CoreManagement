/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <mutex>
#include <tuple>
#include "..\CoreSDK\inc\types.h"
#include "Config.h"

enum class ActiveHorseType : int { Mounted, Active, Both };

class MovementVelocityTracker {
    float m_PlayerFootSum = 0.0f;
    int m_PlayerFootSamples = 0;

    float m_HorseSum = 0.0f;
    int m_HorseImmobileSamples = 0;
    int m_HorseLedSamples = 0;
    int m_HorseSamples = 0;

    std::mutex m_Lock;

    float GetHorseDistanceFromPlayer(Ped playerPed, Ped horsePed) noexcept;
	std::tuple<Ped, bool> GetLedHorseFromPlayer(Ped playerPed) noexcept;
	void RecordPlayerSample(Ped playerPed) noexcept;
	void RecordHorseSample(Ped playerPed) noexcept;
    

public:
    void RecordSnapshot(Ped playerPed) noexcept;

    [[nodiscard]] std::tuple<Ped, bool> GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType = ActiveHorseType::Both) noexcept;
    [[nodiscard]] float GetPlayerOnFootSpeed() noexcept;
    [[nodiscard]] float GetHorseAverageSpeed() noexcept;
    [[nodiscard]] bool IsHorseHitched(const CoreConfigSettings& coreConfig) noexcept;
    [[nodiscard]] bool IsHorseBeingLed(const CoreConfigSettings& coreConfig) noexcept;
    [[nodiscard]] void Reset() noexcept;

};
