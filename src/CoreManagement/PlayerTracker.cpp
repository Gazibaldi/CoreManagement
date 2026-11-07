/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#define NOMINMAX

#include <algorithm>
#include "..\..\inc\natives.h"
#include "PlayerTracker.h"
#include "Logger.h"
#include "HorseManager.h"
#include "Common.h"
#include "CMTypes.h"

const float PLAYER_DIRT_THRESHOLD = 0.5f;
const float PLAYER_CAMP_THRESHOLD = 0.66f;

const float WEIGHT_PLAYER_OVERWEIGHT_THRESHOLD = 83.0f;
const float WEIGHT_PLAYER_UNDERWEIGHT_THRESHOLD = 17.0f;

const int SECONDS_PER_HOUR = 3600;

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

    if (IsPlayerAtCamp(playerPed, true, false)) {
		LOG_DEV("Player is at camp during this snapshot.");
        m_PlayerCampSamples++;
    }
        

    m_PlayerAccumulatedDirt += PED::_GET_PED_DIRT_LEVEL(playerPed, true);

    // Check if the player is mounted on a horse
    auto [horsePed, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed, ActiveHorseType::Mounted);
    if (hasHorse) {
        float horseSpeed = ENTITY::GET_ENTITY_SPEED(horsePed);

        horseSpeed = std::max(horseSpeed, 0.0f);

        m_HorseSum += horseSpeed;
        m_HorseSamples++;

		LOG_DEV("Player is mounted on a horse. Current horse speed: {:.4f} m/s.", horseSpeed);
    }
    else {
        float playerSpeed = ENTITY::GET_ENTITY_SPEED(playerPed);

        playerSpeed = std::max(playerSpeed, 0.0f);

        m_PlayerFootSum += playerSpeed;
        m_PlayerFootSamples++;

        LOG_DEV("Player is on foot. Current on-foot speed: {:.4f} m/s.", playerSpeed);
    }

	m_PlayerDrunknessSum += ATTRIBUTE::GET_ATTRIBUTE_POINTS(playerPed, ePedAttribute::SA_INEBRIATED);

    if (m_LastSleptHours == -1) {
        // Give the player the benefit of the doubt if we don't have a record of their last sleep. We assume they are not tired.
        // But we set the last slept time to 6am (or now if prior to 6am) this morning to start the tiredness clock ticking for future checks.
		int hoursSinceBaseYear = CLOCK::_GET_SECONDS_SINCE_BASE_YEAR() / SECONDS_PER_HOUR;

        int currentHour = CLOCK::GET_CLOCK_HOURS();
        if (currentHour < 6) {
            m_LastSleptHours = hoursSinceBaseYear;
        }
        else {
            // Get the number of full days since the base year (truncate to integer day then back to full day hours, then add 6)
			m_LastSleptHours = (static_cast<int>(hoursSinceBaseYear / 24) * 24) + 6;
        }

        m_LastSleptAtNight = true;

        LOG_DEV("No record of last sleep. Setting last slept time to {} hours since base year (based on current hours since base year {}).", m_LastSleptHours, hoursSinceBaseYear);
    }
}

void PlayerTracker::RecordLastSleptDetails(bool sleptAtNight) noexcept {
    m_LastSleptHours = CLOCK::_GET_SECONDS_SINCE_BASE_YEAR() / SECONDS_PER_HOUR;
    m_LastSleptAtNight = sleptAtNight;

	LOG_DEV("Recording last slept details: hoursSinceBaseYear={}, sleptAtNight={}", m_LastSleptHours, sleptAtNight);
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

    LOG_DEV("Checking if player is dirty. Dirt level: {:.4f}, AccumulatedDirt: {}, TotalSamples: {}, Threshold: {}, IsDirty: {}", dirtLevel, m_PlayerAccumulatedDirt, m_PlayerSamples, PLAYER_DIRT_THRESHOLD, isDirty);

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
        else {
            float averageCampTime = static_cast<float>(m_PlayerCampSamples / m_PlayerSamples);

            isAtCamp = averageCampTime >= PLAYER_CAMP_THRESHOLD;
            LOG_DEV("Checking if player is at camp. Average camp time: {:.4f}, CampSamples: {}, TotalSamples: {}, Threshold: {}, IsAtCamp: {}", averageCampTime, m_PlayerCampSamples, m_PlayerSamples, PLAYER_CAMP_THRESHOLD, isAtCamp);
        }        
    }

    if (logOutcome) {
        if (isAtCamp)
            LOG(LogLevel::Verbose, "Player camp check: Player is at camp (average over drain tick).");
        else
            LOG(LogLevel::Verbose, "Player camp check: Player is not at camp (average over drain tick).");
    }

    return isAtCamp;
}

[[nodiscard]] float PlayerTracker::GetPlayerTiredness(Ped playerPed) noexcept {
	// non-night sleep is tiredness starting at 16 hours after last sleep
	// night sleep is tiredness starting at 24 hours after last sleep
	// each hour of tiredness adds a 0.1 multiplier to dead eye decay, up to a maximum of 1.0 (10 hours of tiredness)

	// sanity check: if the player has never slept, we return 0 tiredness
	// should never happen as we initialize the last slept time to 6am today (or as close as possible) on first snapshot if we have no record of last sleep, but just in case
    if (m_LastSleptHours < 0)
		return 0.0f;

	int hoursSinceBaseYear = CLOCK::_GET_SECONDS_SINCE_BASE_YEAR() / SECONDS_PER_HOUR;

	float hoursSinceLastSleep = static_cast<float>(hoursSinceBaseYear - m_LastSleptHours);
	float tirednessThreshold = m_LastSleptAtNight ? 24.0f : 16.0f;

	LOG_DEV("Calculating player tiredness. Last slept at: {} hours since base year, Current time: {} hours since base year, Hours since last sleep: {:.4f}, Tiredness threshold: {:.4f}", m_LastSleptHours, hoursSinceBaseYear, hoursSinceLastSleep, tirednessThreshold);

    if (hoursSinceLastSleep > tirednessThreshold)
		return std::min((hoursSinceLastSleep - tirednessThreshold) * 0.1f, 1.0f);

    return 0.0f;
}

[[nodiscard]] float PlayerTracker::GetPlayerWeightModifier(Ped playerPed, CoreIndex core) noexcept {
	// weight is a simple tracker as it only updates every 12 in-game hours so we just return the current weight modifier from the player ped

    int weightIndex = ATTRIBUTE::GET_ATTRIBUTE_POINTS(playerPed, ePedAttribute::SA_BODYWEIGHT);

    bool isOverweight = weightIndex >= WEIGHT_PLAYER_OVERWEIGHT_THRESHOLD;
    bool isUnderweight = weightIndex <= WEIGHT_PLAYER_UNDERWEIGHT_THRESHOLD;

    LOG_DEV("GetPlayerWeightModifier: Weight index={}, Overweight={}, Underweight={}", weightIndex, isOverweight, isUnderweight);

    float modifier = 0.0f;

	switch (core) {
	    case CoreIndex::Health:
        case CoreIndex::Stamina:
		    // If the player is overweight or underweight, we apply a 25% penalty to health and stamina decay.
			modifier = isOverweight || isUnderweight ? 0.25f : 0.0f;

			if (modifier > 0.0f)
				LOG(LogLevel::Verbose, "Player weight modifier applied for {} core: {:.2f}, Player Weight={}", (core == CoreIndex::Health ? "Health" : "Stamina"), modifier, isOverweight ? "Overweight" : "Underweight");
			else
				LOG(LogLevel::Verbose, "No player weight modifier applied for {} core., Player Weight is Perfect/Average", (core == CoreIndex::Health ? "Health" : "Stamina"), modifier);

            break;
			// For Dead Eye, we do not apply any weight modifier as it is not affected by the player's weight.
			// this is a design choice as Dead Eye is more about the player's focus and skill rather than their physical condition.
            // non-vanilla but meh!
	    default: 
            break;
	}
    
    return modifier;
}

[[nodiscard]] float PlayerTracker::GetPlayerDrunknessStaminaModifier(Ped playerPed) noexcept {
    int drunkness = 0;

    if (m_PlayerDrunknessSum == 0.0f || m_PlayerSamples == 0) {
        LOG_DEV("No drunkness samples recorded. returning 0.0f.");
        drunkness = 50;
    }
    else {
        // average drunkness level over the samples taken during the drain tick
        drunkness = static_cast<int>(m_PlayerDrunknessSum / m_PlayerSamples);
    }

	// 50 is the baseline for drunkness. Below or equal to 50 is considered not drunk, 
    // and above 50 is considered drunk. The penalty scales with the level of drunkness.
    if (drunkness <= 50) {
		LOG(LogLevel::Verbose, "Player is not drunk. No penalty applied to stamina decay.");
        return 0.0f;
    }

	// We apply a growing penalty to stamina decay based on the drunkness level. The penalty is 0.1% for every point above 50, capped at a maximum of 5% (0.05).
	float modifier = std::min((drunkness - 50) * 0.001f, 0.05f);
	LOG(LogLevel::Verbose, "Player is drunk. Average Drunkness level: {}, Stamina decay penalty applied: {:.4f}", drunkness, modifier);

	return modifier;
}

[[nodiscard]] PlayerSaveState PlayerTracker::GetPlayerSaveState() const noexcept {
	PlayerSaveState state;
	state.LastSleptHours = m_LastSleptHours;
	state.LastSleptAtNight = m_LastSleptAtNight;
	LOG_DEV("Getting player save state: LastSleptHours={}, LastSleptAtNight={}", state.LastSleptHours, state.LastSleptAtNight);
	return state;
}
void PlayerTracker::SetPlayerSaveState(const PlayerSaveState& state) noexcept {
	m_LastSleptHours = state.LastSleptHours;
	m_LastSleptAtNight = state.LastSleptAtNight;
	LOG_DEV("Setting player save state: LastSleptHours={}, LastSleptAtNight={}", m_LastSleptHours, m_LastSleptAtNight);
}

void PlayerTracker::ResetSnapshot(bool includeSleep) noexcept {
	LOG_DEV("Resetting PlayerTracker state.");

	m_PlayerSamples = 0;
	m_PlayerAccumulatedDirt = 0.0f;
	m_PlayerCampSamples = 0;

	m_PlayerFootSum = 0.0f;
	m_PlayerFootSamples = 0;

	m_HorseSum = 0.0f;
	m_HorseSamples = 0;

    m_PlayerDrunknessSum = 0.0f;
	
    if (includeSleep) {
		m_LastSleptHours = -1;
		m_LastSleptAtNight = false;
	}
}