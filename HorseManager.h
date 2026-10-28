/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once

#include <mutex>
#include <tuple>
#include "..\CoreSDK\inc\types.h"

enum class ActiveHorseType : int { Mounted, Active, Either };

class HorseManager {
    int m_HorseImmobileSamples = 0;
    int m_HorseLeadSamples = 0;
    int m_HorseSamples = 0;

	float m_HorseHitchedReward = 0.0f;
	float m_HorseLeadReward = 0.0f;

    float m_HorseAccumulatedDirt = 0.0f;

    std::mutex m_Lock;

    enum class HorseSampleType : int { Lead, Hitched };

    std::tuple<Ped, bool> GetPlayerMountedHorse(Ped playerPed) noexcept;
    std::tuple<Ped, bool> GetPlayerActiveHorse(Ped playerPed, bool includeMounted) noexcept;

    float GetHorseDistanceFromPlayer(Ped playerPed, Ped horsePed) noexcept;
    bool IsBeingLedByPlayer(Ped playerPed, Ped horsePed) noexcept;
    bool TestSampleThreshold(int samples, HorseSampleType sampleType) noexcept;
	void ResetState() noexcept;

public:
    void RecordSnapshot(Ped playerPed) noexcept;

    [[nodiscard]] std::tuple<Ped, bool> GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType = ActiveHorseType::Either) noexcept;
    [[nodiscard]] std::tuple<Ped, bool> GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType, bool performDistanceCheck) noexcept;
    [[nodiscard]] std::tuple<Ped, bool> GetActivePlayerHorse(Ped playerPed, bool performDistanceCheck) noexcept;
    
    [[nodiscard]] bool IsHorseHitched() noexcept;
    [[nodiscard]] bool IsHorseBeingLed() noexcept;
    
    [[nodiscard]] float GetHorseHitchedReward(float hoursDelta) noexcept;
    [[nodiscard]] float GetHorseLeadReward(float hoursDelta) noexcept;

    [[nodiscard]] bool IsHorseDirty(Ped horsePed) noexcept;
    
    void ResetSnapshot() noexcept;
};

extern HorseManager g_HorseManager;