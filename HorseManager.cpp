/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/
#include <tuple>
#include <string>
#include "..\CoreSDK\inc\natives.h"
#include "HorseManager.h"
#include "Logger.h"

HorseManager g_HorseManager;

const float HORSE_MAX_DISTANCE_FROM_PLAYER = 400.0f; // metres
const float HORSE_SAMPLING_THRESHOLD = 0.75f; // 75% of samples must tracked to consider the horse hitched/led
const float HORSE_HITCHED_REWARD_PER_GAME_HOUR = 5.0f;
const float HORSE_LEAD_REWARD_PER_GAME_HOUR = 10.0f;

const float HORSE_DIRT_THRESHOLD = 0.5f; // Arbitrary threshold for determining if a horse is considered dirty

std::tuple<Ped, bool> HorseManager::GetPlayerMountedHorse(Ped playerPed) noexcept {
    Ped activeHorse = PED::GET_MOUNT(playerPed);

    if (activeHorse != 0 &&
        ENTITY::DOES_ENTITY_EXIST(activeHorse) &&
        !ENTITY::IS_ENTITY_DEAD(activeHorse)) {
            LOG_DEV("Player is mounted on a horse.");
            return { activeHorse, true };
    }
    else {
        LOG_DEV("Player is not mounted on a horse.");
        return { 0, false };
    }
}

std::tuple<Ped, bool> HorseManager::GetPlayerActiveHorse(Ped playerPed, bool includeMounted) noexcept {
	auto [mountedHorse, hasHorse] = GetPlayerMountedHorse(playerPed);

	//We check if the player is mounted and if we are not including mounted horses, return no horse
	if (hasHorse && !includeMounted) {
		LOG_DEV("Player is mounted on a horse, but mounted horses are not included in the search. Returning no active horse.");
		return { 0, false };
	}
	// If the player is mounted and we are including mounted horses, return the mounted horse
    else if (hasHorse) {
        return { mountedHorse, hasHorse };
    }
	// If the player is not mounted, check for an active saddled horse in the world
    else {
        Ped activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());
        if (activeHorse != 0 &&
            ENTITY::DOES_ENTITY_EXIST(activeHorse) &&
            !ENTITY::IS_ENTITY_DEAD(activeHorse)) {
            LOG_DEV("Player has an active saddled horse.");
            return { activeHorse, true };
        }
        else {
            LOG_DEV("Player does not have an active saddled horse.");
            return { 0, false };
        }
    }
}

float HorseManager::GetHorseDistanceFromPlayer(Ped playerPed, Ped horsePed) noexcept {
    Vector3 playerCoords = ENTITY::GET_ENTITY_COORDS(playerPed, true, true);
    Vector3 horseCoords = ENTITY::GET_ENTITY_COORDS(horsePed, true, true);

    // Calculate distance in metres (using 3D distance)
    float distance = MISC::GET_DISTANCE_BETWEEN_COORDS(
        playerCoords.x, playerCoords.y, playerCoords.z,
        horseCoords.x, horseCoords.y, horseCoords.z,
        true
    );

    return distance;
}

bool HorseManager::IsBeingLedByPlayer(Ped playerPed, Ped horsePed) noexcept {
    if (!TASK::_IS_PED_LEADING_HORSE(playerPed)) return false;

    Ped ledHorse = TASK::_GET_LED_HORSE_FROM_PED(playerPed);

    LOG_DEV("Checking if horse being led is active horse");

	return ledHorse == horsePed;
}

bool HorseManager::TestSampleThreshold(int samples, HorseSampleType sampleType) noexcept {
    std::lock_guard<std::mutex> guard(m_Lock);

    if (samples == 0 || m_HorseSamples == 0) {
		if (sampleType == HorseSampleType::Lead)
            WriteLog(LogLevel::Verbose, "No lead samples recorded. Horse is not considered led.");
		else if (sampleType == HorseSampleType::Hitched)
			WriteLog(LogLevel::Verbose, "No hitched samples recorded. Horse is not considered hitched.");
        return false;
    }

    bool result = (static_cast<float>(samples) / static_cast<float>(m_HorseSamples)) >= HORSE_SAMPLING_THRESHOLD;

    if(!result)
        LOG_DEV("Horse didn't pass sample threshold check ({}): SamplesRecorded={}, TotalSamplesTaken={}, Threshold={}", std::string(sampleType == HorseSampleType::Lead ? "Lead" : "Hitched"), samples, m_HorseSamples, HORSE_SAMPLING_THRESHOLD);

    return result;
}

void HorseManager::ResetState() noexcept {
    m_HorseSamples = 0;
    m_HorseImmobileSamples = 0;
    m_HorseLeadSamples = 0;
    m_HorseHitchedReward = 0.0f;
    m_HorseLeadReward = 0.0f;
}

[[nodiscard]] std::tuple<Ped, bool> HorseManager::GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType) noexcept {

    switch (horseType) {
	    case ActiveHorseType::Mounted:
		    return GetPlayerMountedHorse(playerPed);
		    break;
	    case ActiveHorseType::Active:
	    case ActiveHorseType::Either:
            auto [activeHorse, hasHorse] = GetPlayerActiveHorse(playerPed, horseType == ActiveHorseType::Either);

            if (hasHorse) {
                float distance = GetHorseDistanceFromPlayer(playerPed, activeHorse);

                LOG_DEV("Active horse detected. Distance from player: {} metres.", distance);

                // If the horse is too far away, we consider it "lost" and reset the state.
                // HORSE_MAX_DISTANCE_FROM_PLAYER (400) metres is a generous distance, 350m is the Bonding Level 4 max whistle distance.
                // Deters exploiting the drain mechanics for the horse by leaving it in a distant location while the player is elsewhere.
                if (distance > HORSE_MAX_DISTANCE_FROM_PLAYER) {
                    WriteLog(LogLevel::Verbose, "Active horse is too far away (" + std::to_string(distance) + " metres). Assuming it is lost.");
                    hasHorse = false;
                    activeHorse = 0;
                }
            }

            return { activeHorse, hasHorse };
		    break;
    }

	return { 0, false }; // Default return if no horse is found
}

[[nodiscard]] bool HorseManager::IsHorseHitched() noexcept {
    LOG_DEV("Getting horse hitched state.");
    
    return TestSampleThreshold(m_HorseImmobileSamples, HorseSampleType::Hitched);
}

[[nodiscard]] bool HorseManager::IsHorseBeingLed() noexcept {
    LOG_DEV("Getting horse being led state.");

    return TestSampleThreshold(m_HorseLeadSamples, HorseSampleType::Lead);
}

[[nodiscard]] float HorseManager::GetHorseHitchedReward(float hoursDelta) noexcept {
	// if the horse is not considered hitched and there is no accumulated reward, we return 0.0f to avoid applying any reward.
	if (!TestSampleThreshold(m_HorseImmobileSamples, HorseSampleType::Hitched) && m_HorseHitchedReward < 1.0f) {
        LOG_DEV("Horse is not considered hitched. No reward to apply.");
		m_HorseHitchedReward = 0.0f; // Reset the accumulated reward if the horse is not hitched

		return 0.0f;
	}

	float reward = HORSE_HITCHED_REWARD_PER_GAME_HOUR * hoursDelta;

	m_HorseHitchedReward += reward; // Accumulate the reward

    LOG_DEV("Current accumulated horse hitched reward: {}", m_HorseHitchedReward);

	if (m_HorseHitchedReward < 1.0f) {
		LOG_DEV("Accumulated horse hitched reward is less than 1.0f. Delaying reward application to next threshold.");

		return 0.0f;
	}

	float rewardToApply = m_HorseHitchedReward;
	m_HorseHitchedReward = 0.0f; // Reset the accumulated reward after applying

	return rewardToApply;
}

[[nodiscard]] float HorseManager::GetHorseLeadReward(float hoursDelta) noexcept {
    if (!TestSampleThreshold(m_HorseLeadSamples, HorseSampleType::Lead) && m_HorseLeadReward < 1.0f) {
        LOG_DEV("Horse is not considered led. No reward to apply.");
        m_HorseLeadReward = 0.0f; // Reset the accumulated reward if the horse is not led

        return 0.0f;
    }

    float reward = HORSE_LEAD_REWARD_PER_GAME_HOUR * hoursDelta;

    m_HorseLeadReward += reward; // Accumulate the reward

	LOG_DEV("Current accumulated horse lead reward: {}", m_HorseLeadReward);

    if (m_HorseLeadReward < 1.0f) {
        LOG_DEV("Accumulated horse lead reward is less than 1.0f. Delaying reward application to next threshold.");

        return 0.0f;
    }

    float rewardToApply = m_HorseLeadReward;
    m_HorseLeadReward = 0.0f; // Reset the accumulated reward after applying

    return rewardToApply;
}

[[nodiscard]] bool HorseManager::IsHorseDirty(Ped horsePed) noexcept {
	float dirtLevel = PED::_GET_PED_DIRT_LEVEL(horsePed, true);

    bool isDirty = dirtLevel > HORSE_DIRT_THRESHOLD;

	LOG_DEV("Checking if horse is dirty. Dirt level: {}, Threshold: {}, IsDirty: {}", dirtLevel, HORSE_DIRT_THRESHOLD, isDirty);

    return isDirty;
}

void HorseManager::RecordSnapshot(Ped playerPed) noexcept {
    LOG_DEV("Recording hitched/led snapshot for horse.");
    std::lock_guard<std::mutex> guard(m_Lock);

    // Check if the player has an active horse (saddled only) not mounted.
	// Hitched horses can't be rewarded if the player is mounted
    // We only want to track the horse's speed if the player is unmounted or leading it
    // mounted speed is tracked in MovementVelocityTracker
    auto [activeHorse, hasHorse] = GetActivePlayerHorse(playerPed, ActiveHorseType::Active);
    if (hasHorse) {
        LOG_DEV("Player has an active horse.");
        
        m_HorseSamples++;
        
        float horseSpeed = ENTITY::GET_ENTITY_SPEED(activeHorse);
        bool isBeingLed = IsBeingLedByPlayer(playerPed, activeHorse);

		// Track immobile horse samples to determine if the horse is hitched
        if (horseSpeed == 0.0f && !isBeingLed) {
            LOG_DEV("Horse is immobile and not being led. Incrementing immobile sample count.");
            m_HorseImmobileSamples++;
        }
        else if(horseSpeed == 0.0f) {
            LOG_DEV("Horse is immobile, but being led by the player (not hitched).");
        }
        else {
			LOG_DEV("Horse is mobile. Current speed: {:.2f} m/s.", horseSpeed);
        }

		// We do a separate check for leading the horse, as it is possible for the horse to be immobile 
        // but still being led by the player (e.g., if the player is holding the reins while standing still).
        if (isBeingLed) {
            LOG_DEV("Player is leading the horse.");
            m_HorseLeadSamples++;
        }
        else {
            LOG_DEV("Player is not leading the horse.");
        }
    }
    else {
        LOG_DEV("Player does not have an active unmounted horse.");
        ResetState();
    }
}

[[nodiscard]] void HorseManager::ResetSnapshot() noexcept {
	LOG_DEV("Resetting horse hitched/led snapshot.");

	std::lock_guard<std::mutex> guard(m_Lock);
    ResetState();
}