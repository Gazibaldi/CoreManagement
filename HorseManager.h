/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once

#include <mutex>
#include <tuple>
#include "Config.h"

enum class ActiveHorseType : int { Mounted, Active, Both };

class HorseManager {
    int m_HorseImmobileSamples = 0;
    int m_HorseLedSamples = 0;

    std::mutex m_Lock;

    float GetHorseDistanceFromPlayer(Ped playerPed, Ped horsePed) noexcept;
    std::tuple<Ped, bool> GetLedHorseFromPlayer(Ped playerPed) noexcept;

public:
    void RecordSnapshot(Ped playerPed) noexcept;

    [[nodiscard]] std::tuple<Ped, bool> GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType = ActiveHorseType::Both) noexcept;
    [[nodiscard]] bool IsHorseHitched(const CoreConfigSettings& coreConfig) noexcept;
    [[nodiscard]] bool IsHorseBeingLed(const CoreConfigSettings& coreConfig) noexcept;
    [[nodiscard]] void ResetHitchedOrLedTracking() noexcept;
};

extern HorseManager g_HorseManager;