/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <mutex>
#include "..\CoreSDK\inc\types.h"

class MovementVelocityTracker {
    float m_PlayerFootSum = 0.0f;
    int m_PlayerFootSamples = 0;

    float m_HorseSum = 0.0f;
    int m_HorseSamples = 0;

    std::mutex m_Lock;

public:
    void RecordSnapshot(Ped playerPed) noexcept;

    [[nodiscard]] float GetPlayerOnFootSpeed() noexcept;
    [[nodiscard]] float GetHorseAverageSpeed() noexcept;
    void ResetSnapshot() noexcept;
};