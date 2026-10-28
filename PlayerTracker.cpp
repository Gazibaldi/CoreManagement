/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#define NOMINMAX

#include <algorithm>
#include <mutex>
#include "..\CoreSDK\inc\natives.h"
#include "PlayerTracker.h"
#include "Logger.h"
#include "HorseManager.h"

const float PLAYER_DIRT_THRESHOLD = 0.5f;

void PlayerTracker::RecordSnapshot(Ped playerPed) noexcept {
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

	// track the player's dirt level to determine if they are dirty
	m_PlayerSamples++;
	m_PlayerAccumulatedDirt += PED::_GET_PED_DIRT_LEVEL(playerPed, true);
}

[[nodiscard]] float PlayerTracker::GetPlayerOnFootSpeed() noexcept {
	LOG_DEV("Getting player average on-foot speed.");
    std::lock_guard<std::mutex> guard(m_Lock);

    if (m_PlayerFootSamples == 0) return 0.0f;

    return m_PlayerFootSum / static_cast<float>(m_PlayerFootSamples);
}

[[nodiscard]] float PlayerTracker::GetHorseAverageSpeed() noexcept {
	LOG_DEV("Getting horse average speed.");
    std::lock_guard<std::mutex> guard(m_Lock);

    if (m_HorseSamples == 0) return 0.0f;

    return m_HorseSum / static_cast<float>(m_HorseSamples);
}

[[nodiscard]] bool PlayerTracker::IsPlayerDirty(Ped playerPed) noexcept {
    std::lock_guard<std::mutex> guard(m_Lock);
    float dirtLevel = 0.0f;

    if (m_PlayerAccumulatedDirt == 0.0f || m_PlayerSamples == 0) {
        LOG_DEV("No dirt samples recorded. Falling back to in the moment check.");

        dirtLevel = PED::_GET_PED_DIRT_LEVEL(playerPed, true);        
    }
    else {
        dirtLevel = m_PlayerAccumulatedDirt / static_cast<float>(m_PlayerSamples);
        
        LOG_DEV("Average player dirt level from samples: {:.4f}", dirtLevel);
    }

    bool isDirty = dirtLevel > PLAYER_DIRT_THRESHOLD;

    LOG_DEV("Checking if player is dirty. Dirt level: {:.4f}, Threshold: {}, IsDirty: {}", dirtLevel, PLAYER_DIRT_THRESHOLD, isDirty);

    return isDirty;
}

void PlayerTracker::ResetSnapshot() noexcept {
	LOG_DEV("Resetting MovementVelocityTracker state.");
	std::lock_guard<std::mutex> guard(m_Lock);

	m_PlayerSamples = 0;
	m_PlayerAccumulatedDirt = 0.0f;

	m_PlayerFootSum = 0.0f;
	m_PlayerFootSamples = 0;

	m_HorseSum = 0.0f;
	m_HorseSamples = 0;
}