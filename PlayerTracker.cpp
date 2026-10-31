/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#define NOMINMAX

#include <algorithm>
#include "..\CoreSDK\inc\natives.h"
#include "PlayerTracker.h"
#include "Logger.h"
#include "HorseManager.h"
#include "Functions.h"

const float PLAYER_DIRT_THRESHOLD = 0.5f;
const float PLAYER_CAMP_THRESHOLD = 0.8f;

void PlayerTracker::ComputeHashes() noexcept {
    m_CampScenarios = { {
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL1") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL2") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL3") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL4") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_GROUND") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SQUAT") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_CRAFT") },
        { GetKey("WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_ARTHUR") },
        { GetKey("WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_JOHN") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_ARTHUR") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_JOHN") },
        { GetKey("WORLD_PLAYER_REST_GROUND") },
        { GetKey("WORLD_PLAYER_REST_GROUND_ARTHUR") },
        { GetKey("WORLD_PLAYER_REST_GROUND_JOHN") },
        { GetKey("PROP_CAMP_FIRE_SIT_ARTHUR") },
        { GetKey("PROP_CAMP_FIRE_SIT_JOHN") },
        { GetKey("WORLD_PLAYER_DYNAMIC_KNEEL") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_ARTHUR") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_JOHN") },
        { GetKey("PROP_PLAYER_SLEEP_A_FRAME_TENT_PLAYER_CAMPS") },
        { GetKey("PROP_CAMP_SLEEP_BEDROLL") },
        { GetKey("PROP_PLAYER_SLEEP_CAMP_BEDROLL") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY_ARTHUR") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY_JOHN") },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND_ARTHUR") },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND_JOHN") },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL") },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL_ARTHUR") },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL_JOHN") },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND") }
    } };
}

void PlayerTracker::RecordSnapshot(Ped playerPed) noexcept {
    if (playerPed == 0 || !ENTITY::DOES_ENTITY_EXIST(playerPed)) return;
	
    LOG_DEV("Recording player snapshot.");

    m_PlayerSamples++;

    if (IsPlayerAtCamp(playerPed, true, false))
        m_PlayerCampSamples++;

    // track the player's dirt level to determine if they are dirty
    m_PlayerAccumulatedDirt += PED::_GET_PED_DIRT_LEVEL(playerPed, true);

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

    if (m_LastSleptSeconds == -1) {
        // Give the player the benefit of the doubt if we don't have a record of their last sleep. We assume they are not tired.
        // But we set the last slept time to 6am (or now if prior to 6am) this morning to start the tiredness clock ticking for future checks.
		int secondsSinceBaseYear = CLOCK::_GET_SECONDS_SINCE_BASE_YEAR();

        int currentHour = CLOCK::GET_CLOCK_HOURS();
        if (currentHour < 6) {
            m_LastSleptSeconds = secondsSinceBaseYear;
        }
        else {
            int daysSinceBaseYear = (secondsSinceBaseYear / 3600) / 24;
            m_LastSleptSeconds = ((daysSinceBaseYear * 24) + 6) * 3600;
        }

        m_LastSleptAtNight = true;

        LOG_DEV("No record of last sleep. Setting last slept time to {} seconds since base year (based on current seconds since base year {}).", m_LastSleptSeconds, secondsSinceBaseYear);
    }
}

void PlayerTracker::RecordLastSleptDetails(int secondsSinceBaseYear, bool sleptAtNight) noexcept {
	LOG_DEV("Recording last slept details: secondsSinceBaseYear={}, sleptAtNight={}", secondsSinceBaseYear, sleptAtNight);

    m_LastSleptSeconds = secondsSinceBaseYear;
    m_LastSleptAtNight = sleptAtNight;
}

[[nodiscard]] float PlayerTracker::GetPlayerOnFootSpeed() noexcept {
	LOG_DEV("Getting player average on-foot speed.");

    if (m_PlayerFootSamples == 0) return 0.0f;

    return m_PlayerFootSum / static_cast<float>(m_PlayerFootSamples);
}

[[nodiscard]] float PlayerTracker::GetHorseAverageSpeed() noexcept {
	LOG_DEV("Getting horse average speed.");

    if (m_HorseSamples == 0) return 0.0f;

    return m_HorseSum / static_cast<float>(m_HorseSamples);
}

[[nodiscard]] bool PlayerTracker::IsPlayerDirty(Ped playerPed) noexcept {
    float dirtLevel = 0.0f;

    if (m_PlayerAccumulatedDirt == 0.0f || m_PlayerSamples == 0) {
        LOG_DEV("No dirt samples recorded. Falling back to in the moment check.");

        dirtLevel = PED::_GET_PED_DIRT_LEVEL(playerPed, true);        
    }
    else {
        dirtLevel = m_PlayerAccumulatedDirt / static_cast<float>(m_PlayerSamples);
    }

    bool isDirty = dirtLevel >= PLAYER_DIRT_THRESHOLD;

    LOG_DEV("Checking if player is dirty. Dirt level: {:.4f}, Threshold: {}, IsDirty: {}", dirtLevel, PLAYER_DIRT_THRESHOLD, isDirty);

    return isDirty;
}

[[nodiscard]] bool PlayerTracker::IsPlayerAtCamp(Ped playerPed, bool rightNow, bool logOutcome) noexcept {
    bool isAtCamp = false;

    // Check if the player is actively at a camp scenario right now
    if (rightNow) {
        isAtCamp = IsInActiveScenario(playerPed, m_CampScenarios, "Camp");
    }
    // we test if the player has been at camp recently by checking the accumulated samples
    else {
        if (m_PlayerCampSamples == 0.0f || m_PlayerSamples == 0) {
            LOG_DEV("No camp samples recorded. Assuming not at camp.");
            isAtCamp = false;
        }

        float averageCampTime = static_cast<float>(m_PlayerCampSamples / m_PlayerSamples);

        isAtCamp = averageCampTime >= PLAYER_CAMP_THRESHOLD;

        LOG_DEV("Checking if player is at camp. Average camp time: {:.4f}, Threshold: {}, IsAtCamp: {}", averageCampTime, PLAYER_CAMP_THRESHOLD, isAtCamp);
    }

    if (logOutcome) {
        if (isAtCamp)
            LOG(LogLevel::Verbose, "Player camp check: Player is at camp.");
        else
            LOG(LogLevel::Verbose, "Player camp check: Player is not at camp.");
    }

    return isAtCamp;
}

[[nodiscard]] float PlayerTracker::GetPlayerTiredness(Ped playerPed, int secondsSinceBaseYear) noexcept {
	// non-night sleep is tiredness starting at 16 hours after last sleep
	// night sleep is tiredness starting at 24 hours after last sleep
	// each hour of tiredness adds a 0.1 multiplier to dead eye decay, up to a maximum of 1.0 (10 hours of tiredness)

	// sanity check: if the player has never slept, we return 0 tiredness
	// should never happen as we initialize the last slept time to 6am today (or as close as possible) on first snapshot if we have no record of last sleep, but just in case
    if (m_LastSleptSeconds < 0)
		return 0.0f;

	float hoursSinceLastSleep = static_cast<float>(secondsSinceBaseYear - m_LastSleptSeconds) / 3600.0f;
	float tirednessThreshold = m_LastSleptAtNight ? 24.0f : 16.0f;

	LOG_DEV("Calculating player tiredness. Last slept at: {} seconds since base year, Current time: {} seconds since base year, Hours since last sleep: {:.4f}, Tiredness threshold: {:.4f}", m_LastSleptSeconds, secondsSinceBaseYear, hoursSinceLastSleep, tirednessThreshold);

    if (hoursSinceLastSleep > tirednessThreshold)
		return std::min((hoursSinceLastSleep - tirednessThreshold) * 0.1f, 1.0f);

    return 0.0f;
}

void PlayerTracker::ResetSnapshot(bool includeSleepDetails) noexcept {
	LOG_DEV("Resetting PlayerTracker state.");

	m_PlayerSamples = 0;
	m_PlayerAccumulatedDirt = 0.0f;

	m_PlayerFootSum = 0.0f;
	m_PlayerFootSamples = 0;

	m_HorseSum = 0.0f;
	m_HorseSamples = 0;
	
    if (includeSleepDetails) {
		m_LastSleptSeconds = -1;
		m_LastSleptAtNight = false;
	}
}