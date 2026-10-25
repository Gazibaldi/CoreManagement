/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#define NOMINMAX

#include <algorithm>
#include <mutex>
#include "..\CoreSDK\inc\natives.h"
#include "MovementVelocityTracker.h"
#include "Logger.h"
#include "HorseManager.h"

void MovementVelocityTracker::RecordSnapshot(Ped playerPed) noexcept {
    if (playerPed == 0 || !ENTITY::DOES_ENTITY_EXIST(playerPed)) return;
	
    LOG_DEV("Recording movement snapshot for player.");

    std::lock_guard<std::mutex> guard(m_Lock);

    // Check if the player is mounted on a horse
    auto [horsePed, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed, ActiveHorseType::Mounted);
    if (hasHorse) {
        float horseSpeed = ENTITY::GET_ENTITY_SPEED(horsePed);

        horseSpeed = std::max(horseSpeed, 0.0f);

        m_HorseSum += horseSpeed;
        m_HorseSamples++;
    }
    else {
        float playerSpeed = ENTITY::GET_ENTITY_SPEED(playerPed);

        playerSpeed = std::max(playerSpeed, 0.0f);

        m_PlayerFootSum += playerSpeed;
        m_PlayerFootSamples++;
    }
}

[[nodiscard]] float MovementVelocityTracker::GetPlayerOnFootSpeed() noexcept {
	LOG_DEV("Getting player average on-foot speed.");
    std::lock_guard<std::mutex> guard(m_Lock);

    if (m_PlayerFootSamples == 0) return 0.0f;

    return m_PlayerFootSum / static_cast<float>(m_PlayerFootSamples);
}

[[nodiscard]] float MovementVelocityTracker::GetHorseAverageSpeed() noexcept {
	LOG_DEV("Getting horse average speed.");
    std::lock_guard<std::mutex> guard(m_Lock);

    if (m_HorseSamples == 0) return 0.0f;

    return m_HorseSum / static_cast<float>(m_HorseSamples);
}

void MovementVelocityTracker::ResetSnapshot() noexcept {
	LOG_DEV("Resetting MovementVelocityTracker state.");
	std::lock_guard<std::mutex> guard(m_Lock);

	m_PlayerFootSum = 0.0f;
	m_PlayerFootSamples = 0;

	m_HorseSum = 0.0f;
	m_HorseSamples = 0;
}