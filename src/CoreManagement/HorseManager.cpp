/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/
#include <tuple>
#include <string>
#include "..\..\inc\natives.h"
#include "..\..\inc\types.h"
#include "HorseManager.h"
#include "Logger.h"
#include "CMTypes.h"

HorseManager g_HorseManager;

const float HORSE_MAX_DISTANCE_FROM_PLAYER = 400.0f; // metres
const float HORSE_SAMPLING_THRESHOLD = 0.75f; // 75% of samples must tracked to consider the horse hitched/led
const float HORSE_HITCHED_REWARD_PER_GAME_HOUR = 5.0f;
const float HORSE_LEAD_REWARD_PER_GAME_HOUR = 10.0f;

const float HORSE_DIRT_THRESHOLD = 0.5f; // Arbitrary threshold for determining if a horse is considered dirty

const float WEIGHT_PLAYER_OVERWEIGHT_THRESHOLD = 83.0f;

const float WEIGHT_HORSE_OVERWEIGHT_THRESHOLD = 70.0f;
const float WEIGHT_HORSE_MALNOURISHED_THRESHOLD = 9.0f;
const float WEIGHT_HORSE_THIN_THRESHOLD = 29.0f;

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
    if (samples == 0 || m_HorseSamples == 0) {
		if (sampleType == HorseSampleType::Lead)
            LOG(LogLevel::Verbose, "No lead samples recorded. Horse is not considered led.");
		else if (sampleType == HorseSampleType::Hitched)
			LOG(LogLevel::Verbose, "No hitched samples recorded. Horse is not considered hitched.");
        return false;
    }

    bool result = (static_cast<float>(samples) / static_cast<float>(m_HorseSamples)) >= HORSE_SAMPLING_THRESHOLD;

    if(!result)
        LOG_DEV(
            "Horse didn't pass sample threshold check ({}):\n"
            "           SamplesRecorded={},\n"
            "           TotalSamplesTaken={},\n"
            "           Threshold={}", std::string(sampleType == HorseSampleType::Lead ? "Lead" : "Hitched"), samples, m_HorseSamples, HORSE_SAMPLING_THRESHOLD);

    return result;
}

[[nodiscard]] std::tuple<Ped, bool> HorseManager::GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType, bool performDistanceCheck) noexcept {
    switch (horseType) {
	    case ActiveHorseType::Mounted:
		    return GetPlayerMountedHorse(playerPed);
		    break;
	    case ActiveHorseType::Active:
	    case ActiveHorseType::Either:
            auto [activeHorse, hasHorse] = GetPlayerActiveHorse(playerPed, horseType == ActiveHorseType::Either);

            if (hasHorse && performDistanceCheck) {
                float distance = GetHorseDistanceFromPlayer(playerPed, activeHorse);

                LOG_DEV("Active horse detected. Distance from player: {:.4f} metres.", distance);

                // If the horse is too far away, we consider it "lost" and reset the state.
                // HORSE_MAX_DISTANCE_FROM_PLAYER (400) metres is a generous distance, 350m is the Bonding Level 4 max whistle distance.
                // Deters exploiting the drain mechanics for the horse by leaving it in a distant location while the player is elsewhere.
                if (distance > HORSE_MAX_DISTANCE_FROM_PLAYER) {
                    LOG(LogLevel::Verbose, "Active horse is too far away ({:.4f} metres). Assuming it is lost.", distance);
                    hasHorse = false;
                    activeHorse = 0;
                }
            }

            return { activeHorse, hasHorse };
		    break;
    }

	return { 0, false }; // Default return if no horse is found
}

[[nodiscard]] std::tuple<Ped, bool> HorseManager::GetActivePlayerHorse(Ped playerPed, ActiveHorseType horseType) noexcept {
	return GetActivePlayerHorse(playerPed, horseType, true);
}
[[nodiscard]] std::tuple<Ped, bool> HorseManager::GetActivePlayerHorse(Ped playerPed, bool performDistanceCheck) noexcept {
	return GetActivePlayerHorse(playerPed, ActiveHorseType::Either, performDistanceCheck);
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
    float dirtLevel = 0.0f;

	if (m_HorseAccumulatedDirt == 0.0f || m_HorseSamples == 0) {
		LOG_DEV("No dirt samples recorded. Falling back to in the moment check.");

        dirtLevel = PED::_GET_PED_DIRT_LEVEL(horsePed, true);
	}
    else {
		dirtLevel = m_HorseAccumulatedDirt / static_cast<float>(m_HorseSamples);
		
        LOG_DEV("Average horse dirt level from samples: {:.4f}", dirtLevel);
    }
	
    bool isDirty = dirtLevel > HORSE_DIRT_THRESHOLD;
	LOG_DEV("Checking if horse is dirty. Dirt level: {:.4f}, Threshold: {}, IsDirty: {}", dirtLevel, HORSE_DIRT_THRESHOLD, isDirty);

    return isDirty;
}

void HorseManager::RecordSnapshot(Ped playerPed) noexcept {
    LOG_DEV("Recording hitched/led snapshot for horse.");

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
			LOG_DEV("Horse is mobile. Current speed: {:.4f} m/s.", horseSpeed);
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

		// We also track the horse's dirt level to determine if it is dirty
        m_HorseAccumulatedDirt += PED::_GET_PED_DIRT_LEVEL(activeHorse, true);
    }
    else {
        LOG_DEV("Player does not have an active unmounted horse.");
        ResetSnapshot();
    }
}

[[nodiscard]] float HorseManager::GetHorseWeightModifier(Ped horsePed, CoreIndex core) noexcept {
    // weight is a simple tracker as it only updates every 12 in-game hours so we just return the current weight modifier from the horse ped

    int weightIndex = ATTRIBUTE::GET_ATTRIBUTE_POINTS(horsePed, ePedAttribute::SA_BODYWEIGHT);

    bool isOverweight = weightIndex >= WEIGHT_HORSE_OVERWEIGHT_THRESHOLD;
    bool isMalnourished = weightIndex <= WEIGHT_HORSE_MALNOURISHED_THRESHOLD;
    bool isThin = weightIndex <= WEIGHT_HORSE_THIN_THRESHOLD && !isMalnourished;

    LOG_DEV("GetHorseWeightModifier: Weight index={}, Overweight={}, Malnourished={}, Thin={}", weightIndex, isOverweight, isMalnourished, isThin);

    float modifier = 0.0f;

    switch (core) {
    case CoreIndex::Health:
        // If the horse is overweight or malnourished, we apply a 25% penalty to health decay. If the horse is thin, we apply a 15% penalty.
        modifier = isOverweight || isMalnourished ? 0.25f : isThin ? 0.15f : 0.0f;

        if (modifier > 0.0f)
            LOG(LogLevel::Verbose, "Horse weight modifier applied for Health core: {:.2f}, Horse Weight={}", modifier, isOverweight ? "Overweight" : isMalnourished ? "Malnourished" : "Thin");
        else
            LOG(LogLevel::Verbose, "No horse weight modifier applied for Health core., Horse Weight is Fit");
        break;

    case CoreIndex::Stamina:
		// If the horse is overweight, we apply a 35% penalty to stamina decay. If the horse is malnourished, we apply a 25% penalty. If the horse is thin, we apply a 10% penalty.
        modifier = isOverweight ? 0.35f : isMalnourished ? 0.25f : isThin ? 0.10f : 0.0f;

        if (modifier > 0.0f)
            LOG(LogLevel::Verbose, "Horse weight modifier applied for Stamina core: {:.2f}, Horse Weight={}", modifier, isOverweight ? "Overweight" : isMalnourished ? "Malnourished" : "Thin");
        else
            LOG(LogLevel::Verbose, "No horse weight modifier applied for Stamina core., Horse Weight is Fit");

        break;

    default:
        break;
    }

    return modifier;
}

[[nodiscard]] float HorseManager::GetMountedPlayerWeightStaminaModifier(Ped horsePed, Ped playerPed) noexcept {
    auto [mountedHorse, isMounted] = GetPlayerMountedHorse(playerPed);
    
    if (isMounted && mountedHorse == horsePed) {
        int weightIndex = ATTRIBUTE::GET_ATTRIBUTE_POINTS(playerPed, ePedAttribute::SA_BODYWEIGHT);

        bool isOverweight = weightIndex >= WEIGHT_PLAYER_OVERWEIGHT_THRESHOLD;

		LOG_DEV("GetMountedPlayerWeightModifier: Weight index={}, Overweight={}", weightIndex, isOverweight);

        if(isOverweight) {
			LOG(LogLevel::Verbose, "Player is overweight while mounted on the horse. Applying a 3% penalty to horse stamina decay.");
			return 0.03f; // Apply a 3% penalty to stamina decay if the player is overweight while mounted on the horse
        }

        LOG(LogLevel::Verbose, "Player is not overweight while mounted on the horse. No penalty applied to horse stamina decay.");

		return 0.0f;
    }

    LOG(LogLevel::Verbose, "Player is not mounted on the horse. No penalty applied to horse stamina decay.");

    return 0.0f;
}

void HorseManager::ResetSnapshot() noexcept {
	LOG_DEV("Resetting horse hitched/led snapshot.");

    m_HorseSamples = 0;
    m_HorseImmobileSamples = 0;
    m_HorseLeadSamples = 0;
    m_HorseHitchedReward = 0.0f;
    m_HorseLeadReward = 0.0f;
    m_HorseAccumulatedDirt = 0.0f;
}