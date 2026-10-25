/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#define NOMINMAX

#include <string>
#include <atomic>
#include <memory>
#include <algorithm>
#include <span>
#include <cstdlib>
#include <array>
#include <cmath>
#include "..\CoreSDK\inc\natives.h"
#include "..\CoreSDK\inc\types.h"
#include "script.h"
#include "Logger.h"
#include "Config.h"
#include "ThreadManager.h"
#include "CMTypes.h"
#include "Constants.h"
#include "TransientActivityMonitor.h"
#include "MovementVelocityTracker.h"
#include "HorseManager.h"
#include "InventoryManager.h"

SimulationState g_State;
MovementVelocityTracker g_MovementTracker;
InventoryManager g_InventoryManager;
TransientActivityRegistry g_activeMonitors;

std::array<Hash, 17> g_SleepScenarios{};
std::array<Hash, 16> g_CampScenarios{};
std::array<Hash, 4> g_TableMinigames{};

void PrecomputeHashes() {
	// Precompute hashes here to avoid repeated calls to MISC::GET_HASH_KEY during gameplay.

    g_SleepScenarios = { {
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL") },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL_ARTHUR") },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL_JOHN") },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND") },
        { GetKey("PROP_PLAYER_SLEEP_BED") }, // hotel room bed
        { GetKey("PROP_PLAYER_SLEEP_BED_ARTHUR") },
        { GetKey("PROP_PLAYER_SLEEP_BED_JOHN") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_ARTHUR") },
        { GetKey("PROP_CAMP_SLEEP_BEDROLL") },
        { GetKey("PROP_PLAYER_SLEEP_CAMP_BEDROLL") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_JOHN") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY_ARTHUR") },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY_JOHN") },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND_ARTHUR") },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND_JOHN") }
    } };

    g_CampScenarios = { {
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL1") },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL2") },
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
        { GetKey("WORLD_PLAYER_DYNAMIC_KNEEL") }
    } };

    g_InventoryManager.ComputeHashes();

	g_TableMinigames = { {
		{ GetKey("poker_sp") },
		{ GetKey("bjack_sp") },
		{ GetKey("dominoes_sp") },
		{ GetKey("fillet_sp") }
	} };

    // additional hashes can be precomputed here if needed
}

void InitialiseActivityMonitors() {
	g_activeMonitors.Insert(TransientActivityType::Bathing, TransientActivityMonitor(
		TransientActivityType::Bathing,
		20, // 20 seconds threshold for bathing reset
		[]() -> bool {
            Ped playerPed = PLAYER::PLAYER_PED_ID();
            std::string bathingStateName = TASK::GET_TASK_MOVE_NETWORK_STATE(playerPed);

			LOG_DEV("Bathing state check: current bathing state={}", bathingStateName);

            return VALID_BATHING_STATES.contains(bathingStateName);
		}
	));
	g_activeMonitors.Insert(TransientActivityType::Jail, TransientActivityMonitor(
		TransientActivityType::Jail,
		22, // 22 seconds threshold for jail reset
		[]() -> bool {
            Player playerId = PLAYER::PLAYER_ID();
            bool isArrested = PLAYER::IS_PLAYER_BEING_ARRESTED(playerId, false);
            bool isInCustody = !PLAYER::IS_PLAYER_PLAYING(playerId);

            LOG_DEV("IsPlayerBeingArrested check: isBeingArrested={}, isInCustody={}", isArrested ? "true" : "false", isInCustody ? "true" : "false");

			return (isArrested && isInCustody);
		}
	));
	// Additional monitors can be added here if needed
}

int GetTotalInGameMinutes() {
    return (CLOCK::GET_CLOCK_HOURS() * 60) + CLOCK::GET_CLOCK_MINUTES();
}

bool IsInActiveScenario(Ped playerPed, std::span<const Hash> scenarioSet, const char* scenarioName) {
    Hash activeScenario = TASK::_GET_SCENARIO_POINT_TYPE_PED_IS_USING(playerPed);
	LOG_DEV("Active scenario hash: {}", activeScenario);

    // Fall back to transient dynamic actions if the primary structural anchor returns empty
	if (activeScenario == 0) {
		activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);
		LOG_DEV("Fallback to dynamic scenario hash: {}", activeScenario);
	}

	if (activeScenario == 0) return false;

    auto contains_check = [activeScenario](Hash scenarioHash) {
        return scenarioHash == activeScenario;
    };

    bool isInScenarioSet = std::ranges::any_of(scenarioSet, contains_check);

	LOG_DEV("Is active scenario ({}) in known set: {}", scenarioName, isInScenarioSet ? "true" : "false");

	return isInScenarioSet;
}

bool IsInSleepScenario(Ped playerPed) {
	bool isInSleepScenario = IsInActiveScenario(playerPed, g_SleepScenarios, "Sleep");

	if (isInSleepScenario)
		LOG(LogLevel::Verbose, "Player sleep check: Player is asleep.");
	else
		LOG(LogLevel::Verbose, "Player sleep check: Player is not asleep.");

	return isInSleepScenario;
}

bool IsInCampScenario(Ped playerPed) {
    bool isInCampScenario = IsInActiveScenario(playerPed, g_CampScenarios, "Camp");
    
    if (isInCampScenario)
        LOG(LogLevel::Verbose, "Player camp check: Player is at camp.");
    else
        LOG(LogLevel::Verbose, "Player camp check: Player is not at camp.");

    return isInCampScenario;
}

bool IsPlayerArrested() {
    TransientActivityMonitor& jailMonitor = g_activeMonitors[TransientActivityType::Jail];

    if (jailMonitor.IsActive()) {
        jailMonitor.MonitorCheck();

        if (jailMonitor.IsExpired()) {
            jailMonitor.Reset();

            LOG(LogLevel::Verbose, "Player jailed check: Player has exited jail. Resetting transient jailed state.");
            return false;
        }

        LOG(LogLevel::Verbose, "Player jailed check: Player is still in jail");
        return true;
    }
    else {
        if(jailMonitor.ActivateMonitor()) {
            LOG(LogLevel::Verbose, "Player jailed check: Player is being arrested and is in custody.");
            return true;
        }

        LOG(LogLevel::Verbose, "Player jailed check: Player is not being arrested or is not in custody.");
        return false;
    }
}

bool IsPlayerBathing(Ped playerPed) {
	// Issue, when purchasing deluxe bathing service, 
    // the player is in a different move network than the maid. 
	// It moving from (BATHING/Scrub_Head/Scrub_Left_Arm/Scrub_Right_Arm/Scrub_Left_Leg/Scrub_Right_Leg to Unknown while the maid cutscene is playing. 
    // 
    // This is annoying because it causes the drain freeze to be lifted during the cutscene, which is not intended as Unknown also states bathing is over.
	// I need to track the state over a period of ticks to confirm the bathing is actually over and not just a transition to the next move network state.

    TransientActivityMonitor& bathingMonitor = g_activeMonitors[TransientActivityType::Bathing];

    if (bathingMonitor.IsActive()) {
        bathingMonitor.MonitorCheck();

        if (bathingMonitor.IsExpired()) {
            bathingMonitor.Reset();

            LOG(LogLevel::Verbose, "Player bathing check: Player has exited the bath. Resetting transient bathing state.");
            return false;
        }

        LOG(LogLevel::Verbose, "Player bathing check: Player is still in a bath");
        return true;
    }
    else {
        if (bathingMonitor.ActivateMonitor()) {
            LOG(LogLevel::Verbose, "Player bathing check: Player is now in a bath.");
            return true;
        }

        LOG(LogLevel::Verbose, "Player bathing check: Player is not in a bath.");
        return false;
    }
}

bool IsPlayerPlayingTableMinigame() {
    for (int idx = 0; idx < g_TableMinigames.size(); ++idx) {
        int threadCount = SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_TableMinigames[idx]);
        LOG_DEV("Table minigame thread count for hash {}: {}", g_TableMinigames[idx], threadCount);
        if (threadCount > 0) {
            LOG(LogLevel::Verbose, "Player is currently playing a table minigame.");
            return true;
        }
    }

    return false;
}

void RecordTrackingSnapshots(Ped playerPed) {
	g_MovementTracker.RecordSnapshot(playerPed);
	g_HorseManager.RecordSnapshot(playerPed);
}

void SetGameTimeScale(int msPerInGameMinute) {
    LOG(LogLevel::Verbose, "Setting game time scale (ms per in-game minute): {}", msPerInGameMinute);
    
	CLOCK::_SET_MILLISECONDS_PER_GAME_MINUTE(msPerInGameMinute);
}

int GetCurrentGameTimeScale() {
    int msPerInGameMinute = CLOCK::GET_MILLISECONDS_PER_GAME_MINUTE();

	LOG_DEV("Current game time scale (ms per in-game minute): {}", msPerInGameMinute);
    return msPerInGameMinute;
}

GameplayContext EvaluateGameplayState(Ped playerPed, const GeneralConfigSettings& generalConfig) {
    GameplayContext ctx;

    bool isInMission = MISC::GET_MISSION_FLAG();
    bool isInMinigame = IsPlayerPlayingTableMinigame();
    bool isFallingOrGoingToDie = PED::IS_PED_FALLING(playerPed) || GRAPHICS::ANIMPOSTFX_IS_RUNNING("MissionFail01");

    LOG_DEV("Gameplay state evaluated: allowDrainInMissions={}, allowDrainInMinigames={}", generalConfig.enableDuringMissions, generalConfig.enableDuringMinigames);

    // If the player is in a mission or minigame and the config disallows drain, we freeze the decay.
    ctx.isInMission = isInMission && !generalConfig.enableDuringMissions;
    ctx.isInMinigame = isInMinigame && !generalConfig.enableDuringMinigames;
    
	// Only check the following states if the player is not already in a mission or minigame
	// they can't be happening/change if we are in mission or minigame or falling/dying
    if (!ctx.isInMission && !ctx.isInMinigame && !isFallingOrGoingToDie) {
        ctx.isBathing = IsPlayerBathing(playerPed);
        ctx.isSleeping = IsInSleepScenario(playerPed);
        ctx.isJailed = IsPlayerArrested();        
        ctx.isAtCamp = IsInCampScenario(playerPed);
    }
    
	int timeScale = GetCurrentGameTimeScale();
	int configuredTimeScale = generalConfig.msPerInGameMinute;

	ctx.isInTimeWarp = (timeScale < VANILLA_GAME_TIME_SCALE);

	// if the player is entering a mission and the time scale is above vanilla, we reset it to vanilla to ensure mission events are not affected by the time scale change. 
	// This is a safeguard against any mission scripts that rely on the vanilla time scale for timing events. 
    if (ctx.isInMission && !g_State.wasInMissionDuringRestriction) {
        LOG(LogLevel::Verbose, "Mission start detected: Setting game time scale to vanilla value.");
		SetGameTimeScale(VANILLA_GAME_TIME_SCALE);
    }
	
	// If the player is in a mission/minigame/timewarp, we don't reset the time scale as it may be intended to be changed during those events.
	// otherwise, we reset it back to the configured value to ensure the drain mechanics are consistent with the player's configuration.
    if (timeScale != configuredTimeScale && !(ctx.isInMission || ctx.isInTimeWarp || ctx.isInMinigame)) {
        LOG(LogLevel::Verbose, "Game time scale has changed to {} probably due to a config change/save load. Setting to configured value: {}.", timeScale, configuredTimeScale);
		SetGameTimeScale(configuredTimeScale);
    }    

    // camp stays out as we want a customized slowed decay in those cases
    ctx.freezeActive = (ctx.isInMission || ctx.isInMinigame || isFallingOrGoingToDie || ctx.isSleeping || ctx.isBathing || ctx.isJailed || ctx.isInTimeWarp);
	ctx.monitorTransientVarsDuringFreeze = ctx.isBathing; // We want to monitor bathing state even when freeze is active, as it can change during the freeze period. (Deluxe bathing cutscene issue)

    if (!ctx.freezeActive)
		RecordTrackingSnapshots(playerPed);
    
	if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev) && HasDevLogCacheChanged(ctx)) {
        // Update tracking baselines immediately
        UpdateDevLogCache(ctx);

        LOG_DEV(
            "Gameplay state changed: \n"
            "           isInMission={},\n"
            "           isInMinigame={},\n"
            "           isFallingOrGoingToDie={},\n"
            "           isBathing={},\n"
            "           isSleeping={},\n"
            "           isAtCamp={},\n"
            "           isJailed={},\n"
            "           isInTimeWarp={},\n"
            "           isFreezeActive={}", 
            ctx.isInMission, ctx.isInMinigame, isFallingOrGoingToDie, ctx.isBathing, ctx.isSleeping, ctx.isAtCamp, ctx.isJailed, ctx.isInTimeWarp, ctx.freezeActive);
    }    

    return ctx;
}

bool IsPlayerMounted(Ped playerPed) {
    auto [horsePed, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed, ActiveHorseType::Mounted);
    
    return hasHorse;
}

float GetPlayerMountedStaminaMultiplier(const PlayerConfigSettings& playerConfig) {
    float currentSpeed = g_MovementTracker.GetHorseAverageSpeed();

    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        LOG(LogLevel::Verbose, "Active horse is galloping. Applying high Player stamina decay multiplier");
        return playerConfig.mountGallopStaminaMultiplier;
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot)) {
        LOG(LogLevel::Verbose, "Active horse is trotting. Applying base Player stamina decay multiplier");
        return playerConfig.mountTrotStaminaMultiplier;
    }
    else {
        LOG(LogLevel::Verbose, "Active horse is walking or stationary. Applying low Player stamina decay multiplier");
        return playerConfig.mountWalkStaminaMultiplier;
    }
}

float GetPlayerRunningStaminaMultiplier(const PlayerConfigSettings& playerConfig) {
	float currentSpeed = g_MovementTracker.GetPlayerOnFootSpeed();

    if (currentSpeed > static_cast<float>(PlayerSpeed::Sprint)) {
        LOG(LogLevel::Verbose, "Player is sprinting. Applying high Player stamina decay multiplier");
        return playerConfig.sprintStaminaMultiplier;
    }
    else if (currentSpeed > static_cast<float>(PlayerSpeed::Jog)) {
        LOG(LogLevel::Verbose, "Player is jogging. Applying medium Player stamina decay multiplier");
        return playerConfig.jogStaminaMultiplier;
    }
    else {
        LOG(LogLevel::Verbose, "Player is walking or stationary. Applying base Player stamina decay multiplier");
        return 1.0f;
    }
}

float CalculateHorseSpeedStaminaDecayMultiplier(const HorseConfigSettings& horseConfig) {
    float currentSpeed = g_MovementTracker.GetHorseAverageSpeed();

    // The logic automatically scales down to a base flat rate if the horse is hitched or stabled!
    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        LOG(LogLevel::Verbose, "Active Horse: Running/Galloping. Applying high stamina multiplier.");
        return horseConfig.gallopStaminaMultiplier;
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot)) {
        LOG(LogLevel::Verbose, "Active Horse: Trotting. Applying medium stamina multiplier.");
        return horseConfig.trotStaminaMultiplier;
    }
    else {
		LOG(LogLevel::Verbose, "Active Horse: Walking/Grazing. Applying base decay.");
        return horseConfig.grazeStaminaMultiplier;
    }
}

float CalculatePlayerHealthDecayMultiplier(const PlayerConfigSettings& playerConfig, float totalPassiveHealthModifierReduction, bool applyPostMissionDecay = false, bool isPlayerSemiRestrained = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained) {
        LOG(LogLevel::Verbose, "Player is semi-restrained. Applying restrained health decay multiplier.");
        multiplier = playerConfig.campedMultiplier;
    }

    if (applyPostMissionDecay) {
        LOG(LogLevel::Verbose, "Player is exiting a mission. Applying post-mission health decay multiplier.");
        multiplier += POST_MISSION_DECAY_ADDITIVE_MULTIPLIER;
    }

    LOG(LogLevel::Verbose, "Active player passive modifiers: Health Reduction={}", totalPassiveHealthModifierReduction);
	multiplier -= totalPassiveHealthModifierReduction;
    
    return std::max(multiplier, 0.1f);
}

float CalculatePlayerStaminaDecayMultiplier(const PlayerConfigSettings& playerConfig, Ped playerPed, float totalPassiveStaminaModifierReduction, bool applyPostMissionDecay = false, bool isPlayerSemiRestrained = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained) {
        LOG(LogLevel::Verbose, "Player is semi-restrained. Applying restrained stamina decay multiplier.");
        multiplier = playerConfig.campedMultiplier;
    }
    else if (IsPlayerMounted(playerPed)) {
        float mountedStaminaMultiplier = GetPlayerMountedStaminaMultiplier(playerConfig);
        
        LOG(LogLevel::Verbose, "Player is mounted. Applying conditional mounted stamina decay multiplier: {}", mountedStaminaMultiplier);
        multiplier = mountedStaminaMultiplier;
    }
    else {
		float runningStaminaMultiplier = GetPlayerRunningStaminaMultiplier(playerConfig);

		LOG(LogLevel::Verbose, "Player is on foot. Applying conditional movement stamina decay multiplier: {}", runningStaminaMultiplier);
		multiplier = runningStaminaMultiplier;
    }

    if (applyPostMissionDecay) {
        LOG(LogLevel::Verbose, "Player is exiting a mission. Applying post-mission stamina decay multiplier.");
        multiplier += POST_MISSION_DECAY_ADDITIVE_MULTIPLIER;
    }

    LOG(LogLevel::Verbose, "Active player passive modifiers: Stamina Reduction = {}", totalPassiveStaminaModifierReduction);
    multiplier -= totalPassiveStaminaModifierReduction;

    return std::max(multiplier, 0.1f);
}

float CalculatePlayerDeadEyeDecayMultiplier(const PlayerConfigSettings& playerConfig, Ped playerPed, float totalPassiveDeadEyeModifierReduction, bool applyPostMissionDecay = false, bool isPlayerSemiRestrained = false, bool isNighttime = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained) {
        LOG(LogLevel::Verbose, "Player is semi-restrained. Applying restrained dead eye decay multiplier.");
        multiplier = playerConfig.campedMultiplier;
    }
    else if (isNighttime) {
        LOG(LogLevel::Verbose, "Player is active at night. Applying night dead eye decay multiplier.");
        multiplier = playerConfig.nightDeadEyeMultiplier;
    }

    float dirtLevel = PED::_GET_PED_DIRT_LEVEL(playerPed, true);

	LOG_DEV("Player dirt level: {}", dirtLevel);

    bool isDirty = dirtLevel > PLAYER_DIRT_THRESHOLD;
    if(isDirty) {
		LOG(LogLevel::Verbose, "Player is dirty. Applying dirty dead eye decay multiplier.");
		multiplier += std::max(playerConfig.deadEyeDirtyMultiplier - 1.0f, 0.0f);
    }    

    if (applyPostMissionDecay) {
        LOG(LogLevel::Verbose, "Player is exiting a mission. Applying post-mission dead eye decay multiplier.");
        multiplier += POST_MISSION_DECAY_ADDITIVE_MULTIPLIER;
    }

    LOG(LogLevel::Verbose, "Active player passive modifiers: Dead Eye Reduction={}", totalPassiveDeadEyeModifierReduction);
    multiplier -= totalPassiveDeadEyeModifierReduction;

    return std::max(multiplier, 0.1f);
}

float CalculatePlayerStaminaSleepReplenish(float currentStamina, float elapsedHours,bool isNighttime, bool isHotelRoom) {
    // Standard linear scaling: 10 points per hour of sleep
    float uplift = elapsedHours * 10.0f;

    // POOR SLEEP PENALTY (Less than 8 Hours or daytime and in the wilderness)
	// If you don't get the minimum of rest, your maximum reward capacity is capped hard at 40 points (20 if it's daytime and you're not in a hotel room).
    if (elapsedHours < MIN_SLEEP_HOURS) {
        float cap = (isNighttime || isHotelRoom) ? 40.0f : 20.0f;
        uplift = std::min(uplift, cap);
        LOG(LogLevel::Standard, "Short sleep session detected (< {} hours). Restorative stamina uplift capped at 40 points.", MIN_SLEEP_HOURS);
    }

	// DAYTIME WILDERNESS SLEEP PENALTY (Minimum Hours but not in a hotel room)
	// If you sleep for the minimum or more hours but it's daytime and you're not in a hotel room, your maximum reward capacity is capped hard at 50 points.
	if (elapsedHours >= MIN_SLEEP_HOURS && (!isNighttime && !isHotelRoom)) {
        uplift = std::min(uplift, 50.0f);
		LOG(LogLevel::Standard, "Daytime wilderness sleep detected. Stamina uplift capped at 50 points.");
	}

	float finalStamina = std::max(currentStamina + uplift, 1.0f);
    return std::min(finalStamina, 100.0f); // Standard full core ceiling
}

float CalculatePlayerDeadEyeSleepReplenish(float currentDeadEye, float elapsedHours, bool isNighttime, bool isHotelRoom) {
    float focusCap = 60.0f;   // Maximum normal sleep reward boundary
    float fatigueCap = 50.0f; // Maximum over-sleep unfocused boundary

    // Luxury Hotel Quality Adjustment (+20/+10 points to caps)
    if (isHotelRoom) {
        if (isNighttime) {
			LOG(LogLevel::Standard, "Luxury hotel room/own bed detected at night. Dead Eye caps increased by +20 points.");
            focusCap += 20.0f;
            fatigueCap += 20.0f;
		}
        else {
            LOG(LogLevel::Standard, "Luxury hotel room/own bed detected during daytime. Dead Eye caps increased by +10 points.");
            focusCap += 10.0f;
            fatigueCap += 10.0f;
        }
    }

	// Daytime Wilderness Penalty (-5 points to caps)
    if (!isNighttime && !isHotelRoom) {
		LOG(LogLevel::Standard, "Daytime wilderness sleep detected. Dead Eye caps decreased by -5 points.");
        focusCap -= 5.0f;
        fatigueCap -= 5.0f;
    }

    // THE OVER-SLEEP UNFOCUS ROUTE (OverSleep Threshold Hours)
    if (elapsedHours >= OVERSLEEP_THRESHOLD_HOURS) {
		// If current dead eye is higher than the fatigue boundary, apply the 15-point reduction penalty (5 points if in a hotel room)
        if (currentDeadEye > fatigueCap) {
            LOG(LogLevel::Standard, "Over-sleep detected. Dead Eye reduced by {} points.", (isHotelRoom ? 5.0f : 15.0f));
            return std::max(currentDeadEye - (isHotelRoom ? 5.0f : 15.0f), fatigueCap);
        }

        return fatigueCap;
    }

    // THE REGULAR SLEEP ROUTE (Less than OverSleep Threshold Hours)
    // If your starting Dead Eye is already higher than the focus cap, leave it completely untouched!
    if (currentDeadEye >= focusCap)
        return currentDeadEye;

    float uplift = elapsedHours * 5.0f;
    float finalDeadEye = std::max(currentDeadEye + uplift, 1.0f);

    return std::min(finalDeadEye, focusCap);
}

float CalculateHorseHealthDecayMultiplier(const PlayerConfigSettings& playerConfig, Ped horsePed, float totalPassiveHealthModifierReduction, bool isPlayerSemiRestrained = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained)
        multiplier = playerConfig.campedMultiplier;
    
    if(g_HorseManager.IsHorseDirty(horsePed))
        multiplier += 0.25f; // Apply a 25% penalty if the horse is dirty

    LOG(LogLevel::Verbose, "Active horse passive modifiers: Health Reduction={}", totalPassiveHealthModifierReduction);
    multiplier -= totalPassiveHealthModifierReduction;    

    return std::max(multiplier, 0.1f);
}

float CalculateHorseStaminaDecayMultiplier(const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig, Ped horsePed, float totalPassiveStaminaModifierReduction, bool isPlayerSemiRestrained = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained)
        multiplier = playerConfig.campedMultiplier;
    else
        multiplier = CalculateHorseSpeedStaminaDecayMultiplier(horseConfig);
    
    if (g_HorseManager.IsHorseDirty(horsePed))
        multiplier += 0.25f; // Apply a 25% penalty if the horse is dirty

    LOG(LogLevel::Verbose, "Active horse passive modifiers: Stamina Reduction={}", totalPassiveStaminaModifierReduction);
    multiplier -= totalPassiveStaminaModifierReduction;

    return std::max(multiplier, 0.1f);
}

float CalculateHorseStaminaSleepReplenish(float currentStamina, float elapsedHours, bool isNighttime, bool isHotelRoom) {
    // Standard linear scaling: 10 points per hour of sleep
    float uplift = elapsedHours * 10.0f;

    // DAYTIME WILDERNESS SLEEP PENALTY (not in a stable (hotel) room at that)
    // If you sleep for the minimum or more hours but it's daytime and you're not in a hotel room, the horse's maximum reward capacity is capped hard at 50 points.
    if (!isNighttime && !isHotelRoom) {
        uplift = std::min(uplift, 50.0f);
        LOG(LogLevel::Standard, "Daytime wilderness sleep detected. Horse Stamina uplift capped at 50 points.");
    }

    float finalStamina = std::max(currentStamina + uplift, 1.0f);
    return std::min(finalStamina, 100.0f); // Standard full core ceiling
}

void ApplyBathingUpdate(Ped playerPed, int elapsedMinutes, float startHealth, float startStamina, float startDeadEye,
    const PlayerConfigSettings& playerConfig) {

    float elapsedHours = static_cast<float>(elapsedMinutes) / 60.0f;

    elapsedHours = std::min(elapsedHours, BATHING_TIMESKIP_MAX_HOURS);

    bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
    bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
    bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    LOG_DEV("Player Bathing Simulation: isHealthGold={}, isStaminaGold={}, isDeadEyeGold={}", isHealthGold, isStaminaGold, isDeadEyeGold);

    CombinedPlayerCoreModifiers passives = g_InventoryManager.GetActivePlayerPassiveModifiers();

    if (!isHealthGold) {
        float activeHealthDecay = playerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(playerConfig, passives.totalHealthReduction);
        float targetHealth = startHealth - (elapsedHours * activeHealthDecay);

        int finalHealth = static_cast<int>(std::max(targetHealth, playerConfig.campedHealthFloor));

        LOG_DEV(
            "Health bathing decay calculation:\n"
            "           CurrentHealth={},\n"
            "           ElapsedHours={:.4f},\n"
            "           ActiveHealthDecay={:.4f},\n"
            "           TargetHealth={},\n"
            "           MinHealthFloor={}", startHealth, elapsedHours, activeHealthDecay, finalHealth, playerConfig.campedHealthFloor);
        LOG(LogLevel::Standard, "Player time-skipped due to bathing. Health decayed to: {}", finalHealth);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

    // Freeze standard drain and apply custom uplift reward
    if (!isStaminaGold) {
        float staminaUplift = 15.0f; // Flat 15 point bonus

        if (g_State.restrictedBathingState == BathingState::DeluxeBathing) {
            LOG(LogLevel::Standard, "Player Bathing Deluxe Service: Stamina bonus doubled!!");
            staminaUplift = 30.0f; // Double bonus for Deluxe Service (Maid)
        }
        else {
			LOG(LogLevel::Standard, "Player Bathing Standard Service: Standard Stamina bonus.");
        }

        int finalStamina = static_cast<int>(std::min(startStamina + staminaUplift, 100.0f));

        LOG_DEV(
            "Stamina bathing uplift calculation:\n"
            "           CurrentStamina={},\n"
            "           ElapsedHours={:.4f},\n"
            "           StaminaUplift={:.4f},\n"
            "           TargetStamina={}", startStamina, elapsedHours, staminaUplift, finalStamina);
        LOG(LogLevel::Standard, "Bathing Stamina Uplift Reward Applied: {}", finalStamina);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
    }

    // Freeze standard drain and apply custom bath metrics
    if (!isDeadEyeGold) {
        // SPECIAL GOLDEN DEADEYE CORE REWARD (Bathing Deluxe Service (Maid))
        if (g_State.restrictedBathingState == BathingState::DeluxeBathing) {
            LOG(LogLevel::Standard, "Player Bathing Deluxe Service: Dead Eye is now golden and fully fortified.");
            ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(playerPed, static_cast<int>(CoreIndex::DeadEye), 100.0f, true);
        }
        else {
			float targetDeadEye = startDeadEye;
			float deadEyeUplift = 0.0f;

            if (startDeadEye < 80.0f) {
                // Calculate the uplift per hour based on the maximum cap of 80.0f over the BATHING_TIMESKIP_MAX_HOURS
				float upliftPerHour = 80.0f / BATHING_TIMESKIP_MAX_HOURS; 

				deadEyeUplift = elapsedHours * upliftPerHour;
                targetDeadEye = std::min(startDeadEye + deadEyeUplift, 80.0f);
                LOG(LogLevel::Standard, "Bathing Standard Service: Dead Eye capped at a maximum baseline of 80.0f.");
            }
            else {
                LOG(LogLevel::Verbose, "Bathing Standard Service: Dead Eye already at or above 80.0f. No uplift applied.");
            }

			int finalDeadEye = static_cast<int>(targetDeadEye);

            LOG_DEV(
                "Dead Eye bathing uplift calculation:\n"
                "           CurrentDeadEye={},\n"
                "           ElapsedHours={:.4f},\n"
                "           DeadEyeUplift={:.4f},\n"
                "           TargetDeadEye={}", startDeadEye, elapsedHours, deadEyeUplift, finalDeadEye);
            LOG(LogLevel::Standard, "Player DeadEye Sync Tick: DeadEye={}", finalDeadEye);
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
        }
    }
}

bool IsSleepDuringTheNight(int elapsedHours, int nightStartHour, int nightEndHour) {
	// Sanity check: If the elapsed hours is less than 1, we don't consider it as sleep during the night.
    if (elapsedHours < 1) {
		LOG(LogLevel::Verbose, "Sleep duration is less than 1 hour. Not considered as sleep during the night.");
		return false;
	}

	int currentHour = CLOCK::GET_CLOCK_HOURS();
    int sleepStartHour = currentHour - elapsedHours;
	if (sleepStartHour < 0) sleepStartHour += 24; // Wrap around if negative

	bool wasNightStart = (sleepStartHour >= nightStartHour || sleepStartHour < nightEndHour);
	bool wasNightEnd = (currentHour >= nightStartHour || currentHour < nightEndHour);

	if (wasNightStart && wasNightEnd) {
		LOG(LogLevel::Verbose, "Player slept completely during the night hours.");
		return true;
	}

	// Calculate the number of hours slept during the night
	int hoursSleptDuringNight = 0;
	
	for (int i = 0; i < elapsedHours; ++i) {
		int hourToCheck = (sleepStartHour + i) % 24;
		if (hourToCheck >= nightStartHour || hourToCheck < nightEndHour)
			hoursSleptDuringNight++;
	}

	if (hoursSleptDuringNight > 0 && hoursSleptDuringNight >= std::lround(elapsedHours * SLEEP_NIGHT_PERCENTAGE_THRESHOLD)) {
		LOG_DEV("Player slept partially during the night hours. Hours slept during night: {}, Total hours slept: {}, Percentage: {:.2f}%", hoursSleptDuringNight, elapsedHours, (static_cast<float>(hoursSleptDuringNight) / elapsedHours) * 100.0f);
		LOG(LogLevel::Verbose, "Player slept partially during the night hours.");

		return true;
	}

	LOG(LogLevel::Verbose, "Player did not sleep during the night hours.");
	return false;
}

void ApplyBatchTimeSkipDecay(
	Ped playerPed, int elapsedMinutes, float startHealth, float startStamina, float startDeadEye, 
    float startHorseHealth, float startHorseStamina, bool processHorse,
	const ConfigSnapshot& config, bool wasSleeping = false, bool wasInMinigame = false) {

    float elapsedHours = static_cast<float>(elapsedMinutes) / 60.0f;

    if (wasInMinigame) {
		LOG(LogLevel::Verbose, "Player is in a minigame. Capping time skip to {} hours for core decay calculations.", MINIGAME_TIMESKIP_MAX_HOURS);
		elapsedHours = std::min(elapsedHours, MINIGAME_TIMESKIP_MAX_HOURS);
    }
    else if (elapsedHours > TIMESKIP_MAX_HOURS) {
		LOG(LogLevel::Verbose, "Time skip exceeds {} hours. Capping to {} hours for core decay calculations.", TIMESKIP_MAX_HOURS, TIMESKIP_MAX_HOURS);
        elapsedHours = TIMESKIP_MAX_HOURS;
    }

	// Post-mission decay is only applied if the player has remaining post-mission decay hours and they are not sleeping
	// This ensures that post-mission decay does not interfere with the sleep replenishment logic
	// minigames are still subject to post-mission decay if the player has remaining hours
    bool applyPostMissionDecay = (g_State.PostMissionDecayHoursRemaining > 0.0f && !wasSleeping);

    if (applyPostMissionDecay) {
        g_State.PostMissionDecayHoursRemaining -= elapsedHours;
        
        // Ensure that the remaining hours do not go below zero
        g_State.PostMissionDecayHoursRemaining = std::max(g_State.PostMissionDecayHoursRemaining, 0.0f);

        LOG(LogLevel::Verbose, "Post-mission decay is active. Remaining hours: {}", g_State.PostMissionDecayHoursRemaining);
    }

    bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
    bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
    bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    LOG_DEV("Player Time Skip Simulation: isHealthGold={}, isStaminaGold={}, isDeadEyeGold={}", isHealthGold, isStaminaGold, isDeadEyeGold);

    CombinedPlayerCoreModifiers passives = g_InventoryManager.GetActivePlayerPassiveModifiers();

	// Health Core Decay Logic
    if (!isHealthGold) {
        float activeHealthDecay = config.PlayerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(config.PlayerConfig, passives.totalHealthReduction, applyPostMissionDecay);
        float targetHealth = startHealth - (elapsedHours * activeHealthDecay);

        float minHealthFloor = wasSleeping ? config.PlayerConfig.healthTimeSkipFloor : config.PlayerConfig.campedHealthFloor;
        
        int finalHealth = BUILTIN::FLOOR(std::max(targetHealth, minHealthFloor));

		LOG_DEV(
            "Health decay calculation:\n"
            "           CurrentHealth={},\n"
            "           ElapsedHours={:.4f},\n"
            "           ActiveHealthDecay={:.4f},\n"
            "           TargetHealth={},\n"
            "           MinHealthFloor={}", startHealth, elapsedHours, activeHealthDecay, finalHealth, minHealthFloor);
        LOG(LogLevel::Standard, "Player time-skipped. Health decayed to: {}", finalHealth);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

    bool sleptAtNight = false;
	bool isHotelRoom = false;

    if (wasSleeping) {
        sleptAtNight = IsSleepDuringTheNight(static_cast<int>(elapsedHours), config.GeneralConfig.nightStartHour, config.GeneralConfig.nightEndHour);
        isHotelRoom = !g_State.wasAtCampDuringRestriction;
    }

	// Stamina Core Replenishment Logic
    if (wasSleeping) {
        // Gold cores are immune to sleep cap constraints, they remain full
        if (!isStaminaGold) {
			// SPECIAL GOLDEN STAMINA CORE REWARD (Minimum Sleep Hours-OverSleep Threshold Hours Hours in a Hotel Room at Night)
            if (isHotelRoom && sleptAtNight && elapsedHours >= MIN_SLEEP_HOURS && elapsedHours <= OVERSLEEP_THRESHOLD_HOURS) {
				
				LOG(LogLevel::Standard, "Player slept {} hours in a hotel room at night. Stamina core is now golden and fully fortified.", elapsedHours);
				ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(playerPed, static_cast<int>(CoreIndex::Stamina), 100.0f, true);
            }
			// REGULAR SLEEP REPLENISHMENT ROUTE (< Minimum Sleep Hours or > OverSleep Threshold Hours and NOT in a Hotel Room at Night)
            else {

                int finalStamina = BUILTIN::FLOOR(CalculatePlayerStaminaSleepReplenish(startStamina, elapsedHours, sleptAtNight, isHotelRoom));

                LOG(LogLevel::Standard, "Player slept. Stamina replenished to: {}", finalStamina);
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
            }
        }
    }
    else if (!isStaminaGold) {
        // Standard macro transit decay (Trains/Stagecoaches/Jail/Barber)
        float activeStaminaDecay = config.PlayerConfig.baseStaminaDecay * CalculatePlayerStaminaDecayMultiplier(config.PlayerConfig, playerPed, passives.totalStaminaReduction, applyPostMissionDecay);

        float targetStamina = startStamina - (elapsedHours * activeStaminaDecay);
        
        int finalStamina = BUILTIN::FLOOR(std::max(targetStamina, config.PlayerConfig.campedStaminaFloor));

        LOG_DEV(
            "Stamina decay calculation:\n"
            "           CurrentStamina={},\n"
            "           ElapsedHours={:.4f},\n"
            "           ActiveStaminaDecay={:.4f},\n"
            "           TargetStamina={},\n"
            "           MinStaminaFloor={}", startStamina, elapsedHours, activeStaminaDecay, finalStamina, config.PlayerConfig.campedStaminaFloor);
        LOG(LogLevel::Standard, "Player time-skipped. Stamina decayed to: {}", finalStamina);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
    }

	// Dead Eye Core Replenishment Logic
    if (wasSleeping) {
        if (!isDeadEyeGold) {
            // Standard sleep replenishment only runs if the core isn't already golden
            int finalDeadEye = BUILTIN::FLOOR(CalculatePlayerDeadEyeSleepReplenish(startDeadEye, elapsedHours, sleptAtNight, isHotelRoom));
            
            LOG(LogLevel::Standard, "Player slept. Dead Eye adjusted to: {}", finalDeadEye);
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
        }
    }
    else if (!isDeadEyeGold) {
        // Standard macro transit decay (Trains/Stagecoaches/Jail/Barber)
		// Don't apply nighttime multiplier here, as the player may have moved across the day/night boundary during the time skip, and we want to avoid over-penalizing them for that.
        float activeDeadEyeDecay = config.PlayerConfig.baseDeadEyeDecay * CalculatePlayerDeadEyeDecayMultiplier(config.PlayerConfig, playerPed, passives.totalDeadEyeReduction, applyPostMissionDecay);

        float targetDeadEye = startDeadEye - (elapsedHours * activeDeadEyeDecay);        
        
        int finalDeadEye = BUILTIN::FLOOR(std::max(targetDeadEye, config.PlayerConfig.campedDeadEyeFloor));

        LOG_DEV(
            "Dead Eye decay calculation:\n"
            "           CurrentDeadEye={},\n"
            "           ElapsedHours={:.4f},\n"
            "           ActiveDeadEyeDecay={:.4f},\n"
            "           TargetDeadEye={},\n"
            "           MinDeadEyeFloor={}", startDeadEye, elapsedHours, activeDeadEyeDecay, finalDeadEye, config.PlayerConfig.campedDeadEyeFloor);
        LOG(LogLevel::Standard, "Player time-skipped. Dead Eye decayed to: {}", finalDeadEye);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
    }

    // Compute Horse Core Reductions
	if (processHorse) {
        auto [activeHorse, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);
        if (hasHorse) {
            bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(activeHorse, static_cast<int>(CoreIndex::Health));
            bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(activeHorse, static_cast<int>(CoreIndex::Stamina));

            LOG_DEV("Horse Time Skip Simulation: isHealthGold={}, isStaminaGold={}", isHorseHealthGold, isHorseStaminaGold);

            CombinedTackModifiers passives = g_InventoryManager.GetActiveHorseTackModifiers(activeHorse);

			// Horse Health Core Decay Logic
            if (!isHorseHealthGold) {
                float activeHealthDecay = config.HorseConfig.baseHealthDecay * CalculateHorseHealthDecayMultiplier(config.PlayerConfig, activeHorse, passives.totalHealthReduction);

                float targetHealth = startHorseHealth - (elapsedHours * activeHealthDecay);
                float minHealthFloor = wasSleeping ? config.HorseConfig.healthTimeSkipFloor : config.HorseConfig.campedHealthFloor;

                int finalHealth = BUILTIN::FLOOR(std::max(targetHealth, minHealthFloor));

                LOG_DEV(
                    "Horse Health decay calculation:\n"
                    "           CurrentHealth={},\n"
                    "           ElapsedHours={:.4f},\n"
                    "           ActiveHealthDecay={:.4f},\n"
                    "           TargetHealth={},\n"
                    "           MinHealthFloor={}", startHorseHealth, elapsedHours, activeHealthDecay, finalHealth, minHealthFloor);
                LOG(LogLevel::Standard, "Player time-skipped. Horse Health decayed to: {}", finalHealth);
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health), finalHealth);
            }

			// Horse Stamina Core Decay Logic
            if (wasSleeping) {
                // Gold cores are immune to sleep cap constraints, they remain full
                if (!isHorseStaminaGold) {
                    bool isHotelRoom = !g_State.wasAtCampDuringRestriction;

                    // SPECIAL GOLDEN STAMINA CORE REWARD (Hotel Stable at Night)
                    if (isHotelRoom && sleptAtNight) {

                        LOG(LogLevel::Standard, "Horse slept in a stable (hotel) at night. Stamina core is now golden and fully fortified.");
                        ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(activeHorse, static_cast<int>(CoreIndex::Stamina), 100.0f, true);
                    }
                    // REGULAR SLEEP REPLENISHMENT ROUTE
                    else {

                        int finalHorseStamina = BUILTIN::FLOOR(CalculateHorseStaminaSleepReplenish(startStamina, elapsedHours, sleptAtNight, isHotelRoom));

                        LOG(LogLevel::Standard, "Player slept. Horse Stamina replenished to: {}", finalHorseStamina);
                        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), finalHorseStamina);
                    }
                }
            }
            else if (!isStaminaGold) {
                // Standard macro transit decay (Trains/Stagecoaches)
                // Normal dynamic movement decay route
                float activeStaminaDecay = config.HorseConfig.baseStaminaDecay * CalculateHorseStaminaDecayMultiplier(config.PlayerConfig, config.HorseConfig, activeHorse, passives.totalStaminaReduction);

                float targetStamina = startHorseStamina - (elapsedHours * activeStaminaDecay);
                
                int finalStamina = BUILTIN::FLOOR(std::max(targetStamina, config.HorseConfig.campedStaminaFloor));

                LOG_DEV(
                    "Horse Stamina decay calculation:\n"
                    "           CurrentStamina={},\n"
                    "           ElapsedHours={:.4f},\n"
                    "           ActiveStaminaDecay={:.4f},\n"
                    "           TargetStamina={},\n"
                    "           MinStaminaFloor={}", startHorseStamina, elapsedHours, activeStaminaDecay, finalStamina, config.HorseConfig.campedStaminaFloor);
                LOG(LogLevel::Standard, "Player time-skipped. Horse Stamina decayed to: {}", finalStamina);
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), finalStamina);
            }
        }
    }

	// Reset the tracker after applying the batch decay to ensure accurate tracking for the next period
	g_MovementTracker.ResetSnapshot();
    g_HorseManager.ResetSnapshot();
}

void UpdateTransientFreezeStateVariables() {
    if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev)) {
        LOG_DEV("Updating frozen state variables based on cached values.");

        LOG_DEV(
            "Frozen state variables updated:\n"
            "           isRestricted                   = {}\n"
            "           isRestrictedVariable           = {}\n"
            "           cachedInGameTimeMinutes        = {}\n"
            "           lastTickInGameMinutes          = {}\n"
            "           wasSleepingDuringRestriction   = {}\n"
            "           wasAtCampDuringRestriction     = {}\n"
            "           wasInMissionDuringRestriction  = {}\n"
            "           wasInMinigameDuringRestriction = {}\n"
            "           wasInTimeWarpDuringRestriction = {}\n"
            "           cachedHealth                   = {}\n"
            "           cachedStamina                  = {}\n"
            "           cachedDeadEye                  = {}\n"
            "           cachedMoney                    = {}\n"
            "           cachedBathingState             = {}\n"
            "           hadHorse                       = {}\n"
            "           cachedHorseHealth              = {}\n"
            "           cachedHorseStamina             = {}",
            g_State.isRestricted,
            g_State.MonitorTransientVariablesDuringRestriction,
            g_State.cachedInGameTimeMinutes,
            g_State.lastTickInGameMinutes,
            g_State.wasSleepingDuringRestriction,
            g_State.wasAtCampDuringRestriction,
            g_State.wasInMissionDuringRestriction,
            g_State.wasInMinigameDuringRestriction,
            g_State.wasInTimeWarpDuringRestriction,
            g_State.cachedHealth,
            g_State.cachedStamina,
            g_State.cachedDeadEye,
            g_State.cachedMoney,
            (g_State.restrictedBathingState == BathingState::NotBathing ? "Not Bathing" :
                g_State.restrictedBathingState == BathingState::StandardBathing ? "Standard Bathing" : "Deluxe Bathing"),
            g_State.hadHorse,
            g_State.cachedHorseHealth,
            g_State.cachedHorseStamina
        );
    }

	// Bathing check: If the player was bathing during the restriction period, we need to determine if they paid for a premium bath and adjust the cached bathing state accordingly.
	if (g_State.restrictedBathingState == BathingState::StandardBathing) {
        if (g_State.cachedMoney > MONEY::_MONEY_GET_CASH_BALANCE()) {
            LOG(LogLevel::Verbose, "Player bathing check: Player has paid for a premium bath. Setting bathing state to Premium Bathing.");
            g_State.restrictedBathingState = BathingState::DeluxeBathing; // Premium Bathing
        }
	}

	// Add more frozen state variable updates here as needed in the future
}

void HandleStateTransitions(Ped playerPed, const GameplayContext& ctx, int currentTotalMinutes, const ConfigSnapshot& config) {
    if (ctx.freezeActive && !g_State.isRestricted) {
        g_State.isRestricted = true;
		g_State.MonitorTransientVariablesDuringRestriction = ctx.monitorTransientVarsDuringFreeze;

        // Lock the context down safely right here!
        g_State.wasSleepingDuringRestriction = ctx.isSleeping;
        g_State.wasAtCampDuringRestriction = ctx.isAtCamp;
		g_State.wasInMissionDuringRestriction = ctx.isInMission;
		g_State.wasInMinigameDuringRestriction = ctx.isInMinigame || ctx.isJailed;
		g_State.wasInTimeWarpDuringRestriction = ctx.isInTimeWarp;
		g_State.restrictedBathingState = ctx.isBathing ? BathingState::StandardBathing : BathingState::NotBathing; // Default to standard bathing state if we're in a bathing scenario, otherwise 0 (not bathing)

        // Cache player base structures
        g_State.cachedHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
        g_State.cachedStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
        g_State.cachedDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));
		g_State.cachedMoney = MONEY::_MONEY_GET_CASH_BALANCE();

		// Cache horse base structures if the player is mounted or we have an active mount in the world
        auto [activeHorse, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);
        if (hasHorse) {
            g_State.hadHorse = true;
            g_State.cachedHorseHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health)));
            g_State.cachedHorseStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina)));
            
            LOG(LogLevel::Verbose, "Entering restricted state. Cached player data and active horse tracking metrics remotely.");
        }
        else {
            g_State.hadHorse = false;
            g_State.cachedHorseHealth = 0.0f;
            g_State.cachedHorseStamina = 0.0f;
            
            LOG(LogLevel::Verbose, "Entering restricted state. Cached player data (No active world mount detected).");
        }
    }

	// Leaving a restricted / fast-forward state (Run Batch Catch-up Calculations)
    if (!ctx.freezeActive && g_State.isRestricted) {
        // We have either just ended one of the following:
		// - A mission or minigame that was blocking core decay
		// - A player death/fall that was blocking core decay
		// - A sleep scenario that was blocking core decay
		// - A bathing scenario that was blocking core decay
		// - A jail scenario that was blocking core decay
        //
		// We now need to calculate the total elapsed time and apply the appropriate decay/replenishment logic.
		// Death is basically redundant here because the player is immediately respawned and the g_state is reset, so we don't need to handle it separately.
		// Sleep/minigames needs to used cached values because the player may have slept for a long time and we need to use the historical values from when they started sleeping.
		// Bathing is a special case as we set up a multiplier that will apply to the slow decay logic, but will only last for the duration of the bathing scenario.
		// missions are a special case as we set up a multiplier that will apply to the slow decay logic, but will only last for POST_MISSION_DECAY_HOURS per mission 
        // (MAX 3 MISSIONS ACCUMULATED) to prevent excessive decay if multiple missions are completed in a short period of time.
		// Minigames/jail exempt the horse from decay as the player is not in control of the horse during those scenarios.
        g_State.isRestricted = false;
		g_State.MonitorTransientVariablesDuringRestriction = false;

        int elapsedMinutes = currentTotalMinutes - g_State.cachedInGameTimeMinutes;

        if (elapsedMinutes < 0)
            elapsedMinutes += 1440; // Self-corrects midnight rollover cleanly

        LOG(LogLevel::Verbose, "Exiting restricted state. Total elapsed game minutes: {}", elapsedMinutes);

        if (elapsedMinutes > 0)
			// Mini-games are table mini-games/jail
            if (g_State.wasSleepingDuringRestriction || g_State.wasInMinigameDuringRestriction) {
                // Uses the HISTORICAL frozen values from the cache
				LOG(LogLevel::Verbose, "Player was sleeping/in a minigame/jailed during restriction. Applying batch time skip decay.");

                ApplyBatchTimeSkipDecay(playerPed, elapsedMinutes,
                    g_State.cachedHealth, g_State.cachedStamina, g_State.cachedDeadEye,
                    g_State.cachedHorseHealth, g_State.cachedHorseStamina, (g_State.hadHorse && !g_State.wasInMinigameDuringRestriction),
                    config,
                    g_State.wasSleepingDuringRestriction, g_State.wasInMinigameDuringRestriction);                
            }
            else if (g_State.restrictedBathingState != BathingState::NotBathing) {
                // Uses the HISTORICAL frozen values from the cache
                LOG(LogLevel::Verbose, "Player was bathing during restriction. Applying batch time skip decay with bathing multiplier.");

                ApplyBathingUpdate(playerPed, elapsedMinutes,
                    g_State.cachedHealth, g_State.cachedStamina, g_State.cachedDeadEye,
                    config.PlayerConfig);
            }
            else if (g_State.wasInMissionDuringRestriction)
            {
                if (elapsedMinutes >= POST_MISSION_MIN_ELAPSED_MINUTES)
                {   g_State.PostMissionDecayHoursRemaining += POST_MISSION_DECAY_HOURS;

                    // Cap the post-mission decay hours to a maximum of 3 hours to prevent excessive decay 
                    // (in-case multiple missions accumulate over the decay multiplier period)
                    g_State.PostMissionDecayHoursRemaining = std::min(g_State.PostMissionDecayHoursRemaining, 3.0f);
                    LOG(LogLevel::Verbose, "Player exited a mission. Post-mission decay hours remaining: {}", g_State.PostMissionDecayHoursRemaining);
                }
                else {
					LOG(LogLevel::Verbose, "Player exited a mission but elapsed time was less than the minimum threshold. No post-mission decay applied.");
                }
            }
            else {
                LOG_DEV("Player was not sleeping, in a minigame, or in a mission during restriction. No batch decay applied.");
            }

		// Reset the transient state variables after processing the batch decay to ensure accurate tracking for the next period
        g_State.wasSleepingDuringRestriction = false;
        g_State.wasInMinigameDuringRestriction = false;
		g_State.wasInMissionDuringRestriction = false;  
        g_State.wasInTimeWarpDuringRestriction = false;
        
        // Reset the cached bathing state and money to prevent false positives on future bathing checks
        g_State.restrictedBathingState = BathingState::NotBathing;
        g_State.cachedMoney = 0;
    }

    // This stops CalculateTimescaleDelta from processing a phantom macro-jump on the next loop tick.
    g_State.cachedInGameTimeMinutes = currentTotalMinutes;
    g_State.lastTickInGameMinutes = g_State.cachedInGameTimeMinutes;
}

float CalculateTimescaleDelta(int currentTotalMinutes) {
    // Safe Initialisation (Fails fast on frame 1 using the unified structure state)
    if (!g_State.timescaleDeltaInitialised) {
        g_State.lastTickInGameMinutes = currentTotalMinutes;
        g_State.timescaleDeltaInitialised = true;

        LOG_DEV("Timescale delta initialised. Baseline in-game minutes: {}", g_State.lastTickInGameMinutes);
        return 0.0f;
    }

    // Fail Fast Optimization
    if (currentTotalMinutes == g_State.lastTickInGameMinutes) return 0.0f;

    int gameMinutesDelta = currentTotalMinutes - g_State.lastTickInGameMinutes;

    // Midnight Rollover Correction
    // If the clock rolled backwards (e.g., from 1439 down to 5 mins), 
    // adding 1440 restores the true forward time delta.
    if (gameMinutesDelta < 0) {
        LOG_DEV("Midnight rollover detected. Adjusting game minutes delta from {} to {}", gameMinutesDelta, gameMinutesDelta + 1440);
        gameMinutesDelta += 1440;
    }

    // UNMANAGED MACRO JUMP INTERCEPT (Trains, Coaches, Fast Travel)
    // If a time skip > 2 hours happens while the player has normal open-world agency,
    // we return a clear invalid value to signal that UpdateCoreSimulation should take over and process the jump.
    if (gameMinutesDelta > 120) {
        LOG_DEV("Large time skip detected. Returning invalid value.");
        return -1.0f;
    }

    return static_cast<float>(gameMinutesDelta) / 60.0f;
}

void ProcessPlayerSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const ConfigSnapshot& config) {
    bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
	bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
	bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    float currentHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
    float currentStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
    float currentDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));

    LOG_DEV("Player Core Simulation: isHealthGold={}, isStaminaGold={}, isDeadEyeGold={}", isHealthGold, isStaminaGold, isDeadEyeGold);

	// Track post-mission decay hours remaining if the player is exiting a mission. This will apply a multiplier to all cores for a limited time after the mission ends.
    bool applyPostMissionDecay = (g_State.PostMissionDecayHoursRemaining > 0.0f);

    if (applyPostMissionDecay) {
        g_State.PostMissionDecayHoursRemaining -= hoursDelta;
		
		// Ensure that the remaining hours do not go below zero
		g_State.PostMissionDecayHoursRemaining = std::max(g_State.PostMissionDecayHoursRemaining, 0.0f);
        
        LOG(LogLevel::Verbose, "Post-mission decay is active. Remaining hours: {}", g_State.PostMissionDecayHoursRemaining);
	}

    CombinedPlayerCoreModifiers passives = g_InventoryManager.GetActivePlayerPassiveModifiers();
    
    if (!isHealthGold) {
        float activeHealthDecay = config.PlayerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(config.PlayerConfig, passives.totalHealthReduction, applyPostMissionDecay, ctx.isAtCamp);

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);

        // The player is semi restrained (jail/camp), so we apply the player's restrained floor to prevent full depletion
		float minHealthFloor = ctx.isAtCamp ? config.PlayerConfig.campedHealthFloor : 0.0f;          
        
        int finalHealth = BUILTIN::FLOOR(std::max(targetHealth, minHealthFloor));

        LOG_DEV(
            "Player Core Simulation:\n"
            "           HoursDelta={:.4f},\n"
            "           HealthDecay={:.4f},\n"
            "           CurrentHealth={},\n"
            "           TargetHealth={},\n"
            "           MinHealthFloor={}", hoursDelta, activeHealthDecay, currentHealth, finalHealth, minHealthFloor);
        LOG(LogLevel::Standard, "Player Core Sync Tick: Health={}", finalHealth);

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

	if (!isStaminaGold) {
        float activeStaminaDecay = config.PlayerConfig.baseStaminaDecay * CalculatePlayerStaminaDecayMultiplier(config.PlayerConfig, playerPed, passives.totalStaminaReduction, applyPostMissionDecay, ctx.isAtCamp);

        float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
        float minStaminaFloor = ctx.isAtCamp ? config.PlayerConfig.campedStaminaFloor : 0.0f;
        
        int finalStamina = BUILTIN::FLOOR(std::max(targetStamina, minStaminaFloor));
            
        LOG_DEV(
            "Player Core Simulation:\n"
            "           HoursDelta={:.4f},\n"
            "           StaminaDecay={:.4f},\n"
            "           CurrentStamina={},\n"
            "           TargetStamina={},\n"
            "           MinStaminaFloor={}", hoursDelta, activeStaminaDecay, currentStamina, finalStamina, minStaminaFloor);
        LOG(LogLevel::Standard, "Player Stamina Sync Tick: Stamina={}", finalStamina);

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
    }

    if (!isDeadEyeGold) {
        // Normal Open-World, Nighttime, or Semi-Restrained Decay Route
        int currentHour = CLOCK::GET_CLOCK_HOURS();
        bool isNighttime = (currentHour >= config.GeneralConfig.nightStartHour || currentHour < config.GeneralConfig.nightEndHour);
        float activeDeadEyeDecay = config.PlayerConfig.baseDeadEyeDecay * CalculatePlayerDeadEyeDecayMultiplier(config.PlayerConfig, playerPed, passives.totalDeadEyeReduction, applyPostMissionDecay, ctx.isAtCamp, isNighttime);

        float targetDeadEye = currentDeadEye - (activeDeadEyeDecay * hoursDelta);
        float minDeadEyeFloor = ctx.isAtCamp ? config.PlayerConfig.campedDeadEyeFloor : 0.0f;

        int finalDeadEye = BUILTIN::FLOOR(std::max(targetDeadEye, minDeadEyeFloor));

        LOG_DEV(
            "Player Core Simulation:\n"
            "           HoursDelta={:.4f},\n"
            "           DeadEyeDecay={:.4f},\n"
            "           CurrentDeadEye={},\n"
            "           TargetDeadEye={},\n"
            "           MinDeadEyeFloor={}", hoursDelta, activeDeadEyeDecay, currentDeadEye, finalDeadEye, minDeadEyeFloor);
        LOG(LogLevel::Standard, "Player DeadEye Sync Tick: DeadEye={}", finalDeadEye);

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
    }
}

void ProcessHorseSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, 
    const ConfigSnapshot& config) {
    auto [horsePed, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);

	if (!hasHorse)
    {
		LOG(LogLevel::Standard, "No active horse detected for the player. Skipping horse core simulation.");
        g_HorseManager.ResetSnapshot();
        return;
    }

    bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Health));
    bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Stamina));

    float currentHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health)));
    float currentStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina)));

    LOG_DEV("Horse Core Simulation: isHealthGold={}, isStaminaGold={}", isHorseHealthGold, isHorseStaminaGold);

    CombinedTackModifiers passives = g_InventoryManager.GetActiveHorseTackModifiers(horsePed);

	// Health Core Decay Logic (only applies if the horse's health core is not golden)
    if (!isHorseHealthGold) {
        float activeHealthDecay = config.HorseConfig.baseHealthDecay * CalculateHorseHealthDecayMultiplier(config.PlayerConfig, horsePed, passives.totalHealthReduction, ctx.isAtCamp);

		float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
		float minHealthFloor = ctx.isAtCamp ? config.HorseConfig.campedHealthFloor : 0.0f; //the player is semi restrained (jail/camp/bath), so we apply the horse's restrained floor to prevent full depletion

        int finalHealth = BUILTIN::FLOOR(std::max(targetHealth, minHealthFloor));

        LOG_DEV(
            "Horse Core Simulation:\n"
            "           HoursDelta={:.4f},\n"
            "           HealthDecay={:.4f},\n"
            "           CurrentHealth={},\n"
            "           TargetHealth={},\n"
            "           MinHealthFloor={}", hoursDelta, activeHealthDecay, currentHealth, finalHealth, minHealthFloor);
        LOG(LogLevel::Standard, "Horse Core Sync Tick: Health={}", finalHealth);

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

	// Stamina Core Decay Logic (only applies if the horse's stamina core is not golden)
    if (!isHorseStaminaGold) {
        float targetStamina = currentStamina;

        // If the horse is physically hitched, freeze its stamina completely!
		// Apply a small bonus if the horse is well-fed or health is golden to reward the player for good care
        // This rewards the player for using hitching posts around the world.
        
        if (g_HorseManager.IsHorseHitched()) {
            LOG(LogLevel::Standard, "Active Horse is securely hitched. Freezing stamina core decay completely.");

            // Give a little bonus to stamina is horse is well fed
            if (isHorseHealthGold || currentHealth >= HORSE_HITCHED_HEALTH_REQ) {
                
				float reward = g_HorseManager.GetHorseHitchedReward(hoursDelta);

				// The reward method doesn't return a reward under 1.0f, so we only apply it if it's a meaningful value
				if (reward >= 1.0f) {
                    int finalStamina = BUILTIN::FLOOR(std::min(currentStamina + reward, 100.0f));

				    LOG(LogLevel::Standard, "Horse is well-fed or has golden health. Applying hitched bonus: +{} to stamina core. Horse Stamina updated to: {}", reward, finalStamina);
                    ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), finalStamina);
				}
            }
        }
		else if (g_HorseManager.IsHorseBeingLed()) {
            float reward = g_HorseManager.GetHorseLeadReward(hoursDelta);

            // The reward method doesn't return a reward under 1.0f, so we only apply it if it's a meaningful value
            if (reward >= 1.0f) {
                int finalStamina = BUILTIN::FLOOR(std::min(currentStamina + reward, 100.0f));

                LOG(LogLevel::Standard, "Active Horse is being led by the player. Applying cached reward: +{}. Horse Stamina updated to: {}", reward, finalStamina);
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), finalStamina);
            }        
		}
        else {
            // Normal dynamic movement decay route
            float activeStaminaDecay = config.HorseConfig.baseStaminaDecay * CalculateHorseStaminaDecayMultiplier(config.PlayerConfig, config.HorseConfig, horsePed, passives.totalStaminaReduction, ctx.isAtCamp);
            
            float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
            float minStaminaFloor = ctx.isAtCamp ? config.HorseConfig.campedStaminaFloor : 0.0f;

            int finalStamina = BUILTIN::FLOOR(std::max(targetStamina, minStaminaFloor));

            LOG_DEV(
                "Horse Core Simulation:\n"
                "           HoursDelta={:.4f},\n"
                "           StaminaDecay={:.4f},\n"
                "           CurrentStamina={},\n"
                "           TargetStamina={},\n"
                "           MinStaminaFloor={}", hoursDelta, activeStaminaDecay, currentStamina, finalStamina, minStaminaFloor);
            LOG(LogLevel::Standard, "Horse Core Sync Tick: Stamina={}", finalStamina);

            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), finalStamina);

			//not hitched or led, so we reset the tracking to ensure accurate decay calculations for the next tick
			g_HorseManager.ResetSnapshot();
        }
    }    
}

void ProcessOpenWorldTimeSkip(Ped playerPed, int currentTotalMinutes, const ConfigSnapshot& config) { 
    int elapsedMinutes = 120; // Since we are in an open-world time skip, we assume a default of 2 hours (120 minutes) for the elapsed time (maps to CalculateTimescaleDelta).
    
    LOG(LogLevel::Verbose, "Open-world time skip detected. Elapsed game minutes: {}", elapsedMinutes);

    // Reads LIVE data from the engine at the moment of travel
    float liveHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
    float liveStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
    float liveDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));

	LOG(LogLevel::Verbose, "Player Live Core Values: Health={}, Stamina={}, DeadEye={}", liveHealth, liveStamina, liveDeadEye);

    // We check horse baseline stats live too
    auto [activeHorse, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);

    float liveHorseHealth = hasHorse ? static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health))) : 0.0f;
    float liveHorseStamina = hasHorse ? static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina))) : 0.0f;

	LOG(LogLevel::Verbose, "Horse Live Core Values: Health={}, Stamina={}, HasHorse={}", liveHorseHealth, liveHorseStamina, hasHorse);

    // Route directly into the shared engine (Never sleeping during open-world transits)
    ApplyBatchTimeSkipDecay(playerPed, elapsedMinutes,
        liveHealth, liveStamina, liveDeadEye,
        liveHorseHealth, liveHorseStamina, hasHorse,
        config);

    // This updates the transition baseline AND keeps CalculateTimescaleDelta from dual-decaying
    g_State.cachedInGameTimeMinutes = currentTotalMinutes;
    g_State.lastTickInGameMinutes = currentTotalMinutes;
}

void ResetSimulationState() {
    g_State.isRestricted = false;
    g_State.MonitorTransientVariablesDuringRestriction = false;

    g_State.cachedInGameTimeMinutes = GetTotalInGameMinutes();
    g_State.lastTickInGameMinutes = 0;
    g_State.timescaleDeltaInitialised = false;
    g_State.stateChangeTickIntervalMsType = StateChangeIntervalType::NotSet;

    g_State.accumulatedTimeMs = 0;
    g_State.cachedHealth = 100.0f;
    g_State.cachedStamina = 100.0f;
    g_State.cachedDeadEye = 100.0f;    
    g_State.cachedMoney = 0;

	g_State.restrictedBathingState = BathingState::NotBathing;

    g_State.cachedHorseHealth = 100.0f;
    g_State.cachedHorseStamina = 100.0f;

    g_State.hadHorse = false;

    g_State.wasSleepingDuringRestriction = false;
    g_State.wasAtCampDuringRestriction = false;
	g_State.wasInMissionDuringRestriction = false;
	g_State.wasInMinigameDuringRestriction = false;
	g_State.wasInTimeWarpDuringRestriction = false;

	g_State.PostMissionDecayHoursRemaining = 0.0f;

    g_State.wasRestrictedLastTick = false;

    g_State.lastKnownPlayerPed = 0;

    g_activeMonitors.ResetAll();

	g_MovementTracker.ResetSnapshot();
    g_HorseManager.ResetSnapshot();

    g_State.configStructuresInitialised = false;    

    ClearDevLogCache();
}

ConfigSnapshot CheckForConfigChange() {

	// If the config has changed OR we haven't initialised the config structures yet, we need to refresh the cache
	// We also check if the player is in a restricted state, as we don't want to refresh the cache while the player 
    // is in a restricted state (e.g., during a mission or minigame)
    if (g_HasConfigChanged.load(std::memory_order_acquire) || !g_State.configStructuresInitialised) {
        if (!g_State.isRestricted) {
            // Log an accurate message based on WHY we are filling the cache
            if (!g_State.configStructuresInitialised)
                LOG(LogLevel::ConfigOnly, "Initialising local simulation configuration cache for the first time...");
            else
                LOG(LogLevel::ConfigOnly, "Dynamic config hot-reload requested. Updating transient systems...");

            g_State.configStructuresInitialised = true;

            g_activeMonitors.RefreshAll(g_State.isRestricted);

            g_HasConfigChanged.store(false, std::memory_order_release);
        }
        else {
			LOG(LogLevel::ConfigOnly, "Dynamic config hot-reload requested, but the player is in a restricted state. Deferring config update until the next unrestricted tick.");
        }
    }

    return ConfigSnapshot {
        g_CoreConfig.load(std::memory_order_acquire),
        g_GeneralConfig.load(std::memory_order_acquire),
        g_PlayerConfig.load(std::memory_order_acquire),
        g_HorseConfig.load(std::memory_order_acquire)
    };
}

int CalculateStateChangeTickIntervalMs() {
    int normalTickInterval = g_StateChangeTickIntervalMs.load(std::memory_order_acquire);
    int restrictedTickInterval = g_StateChangeTickIntervalRestrictedMs.load(std::memory_order_acquire);

    // Safe Initialisation (mainly for logging purposes) - we want to log the first tick interval used to ensure we have a baseline for debugging
    if (g_State.stateChangeTickIntervalMsType == StateChangeIntervalType::NotSet) {
        g_State.stateChangeTickIntervalMsType = StateChangeIntervalType::Standard;
        LOG(LogLevel::Verbose, "State Change Interval Initialised: Using standard tick interval of {} ms", normalTickInterval);
    }

    // If we are entering a restricted state, we want to use the restricted tick interval for faster processing of core simulation
    if (g_State.isRestricted && g_State.stateChangeTickIntervalMsType == StateChangeIntervalType::Standard) {
        g_State.stateChangeTickIntervalMsType = StateChangeIntervalType::Restricted;
        LOG(LogLevel::Verbose, "State Change Interval Change: Restricted state active. Using restricted tick interval of {} ms", restrictedTickInterval);
    }
    // If we are exiting a restricted state, we want to use the standard tick interval for normal gameplay
    else if (!g_State.isRestricted && g_State.stateChangeTickIntervalMsType == StateChangeIntervalType::Restricted) {
        g_State.stateChangeTickIntervalMsType = StateChangeIntervalType::Standard;
        LOG(LogLevel::Verbose, "State Change Interval Change: Restricted state inactive. Using standard tick interval of {} ms", normalTickInterval);
    }

    // If we are in the same state as before, we just return the current tick interval without logging
    if (g_State.stateChangeTickIntervalMsType == StateChangeIntervalType::Standard)
        return normalTickInterval;

    return restrictedTickInterval;
}

bool IsInSaveLoadState(Ped playerPed) {
    Player playerId = PLAYER::PLAYER_ID();

    bool isFreshSaveLoad = (g_State.lastKnownPlayerPed != 0 && g_State.lastKnownPlayerPed != playerPed);
    bool isDead = PLAYER::IS_PLAYER_DEAD(playerId) || !ENTITY::DOES_ENTITY_EXIST(playerPed);
    bool isLoading = DLC::GET_IS_LOADING_SCREEN_ACTIVE();

	LOG_DEV("Save/Load State Check: isFreshSaveLoad={}, isDead={}, isLoading={}", isFreshSaveLoad, isDead, isLoading);

    // HARD RESET ENGINE
    if (isLoading || isDead || isFreshSaveLoad) {
        if (g_State.timescaleDeltaInitialised) {
            LOG(LogLevel::Standard, "Hard Reset Event Verified (Save Load/Death). Purging cache cleanly...");
            ResetSimulationState();
        }

        return true;
    } 
    // RE-ANCHOR ENGINE (Game start)
    else if (!g_State.timescaleDeltaInitialised) {
        LOG(LogLevel::Standard, "Simulation baseline calibrated. Anchoring timestamps...");

        g_State.cachedInGameTimeMinutes = GetTotalInGameMinutes();
        g_State.lastTickInGameMinutes = g_State.cachedInGameTimeMinutes;
        g_State.accumulatedTimeMs = 0;

        g_State.timescaleDeltaInitialised = true;

        CalculateStateChangeTickIntervalMs();
    }

    g_State.lastKnownPlayerPed = playerPed;

    return false;
}

void UpdateCoreSimulation() {
    Ped playerPed = PLAYER::PLAYER_PED_ID();

    if (IsInSaveLoadState(playerPed))
        return;

    ConfigSnapshot config = CheckForConfigChange();

    GameplayContext context = EvaluateGameplayState(playerPed, config.GeneralConfig);
    int currentTotalMinutes = GetTotalInGameMinutes();

    // FAST-PATH BOUNDARY TRANSITIONS (Forcing immediate responses to missions or travel updates)
    if (context.freezeActive != g_State.wasRestrictedLastTick) {
        LOG(LogLevel::Standard, "Fundamental state boundary flipped! Forcing immediate cache transition process.");

        HandleStateTransitions(playerPed, context, currentTotalMinutes, config);

        g_State.wasRestrictedLastTick = context.freezeActive;
        g_State.accumulatedTimeMs = 0;
    }
    // If we are in a restricted state and monitoring transient variables, we need to update the frozen state variables based on the cached values.
    else if (g_State.isRestricted && g_State.MonitorTransientVariablesDuringRestriction) {
        UpdateTransientFreezeStateVariables();

        // Update the last tick time to the current total minutes after processing
        LOG_DEV("Updating last tick in-game minutes from {} to {}", g_State.lastTickInGameMinutes, currentTotalMinutes);
        g_State.lastTickInGameMinutes = currentTotalMinutes;
    }
    // SLOW-PATH APP DECAY SIMULATION (Accumulates interval smoothly over normal open world play)
    else if (!context.freezeActive) {
        g_State.accumulatedTimeMs += config.CoreConfig.stateChangeTickIntervalMs;
        LOG(LogLevel::Verbose, "Accumulated time for core simulation: {} ms", g_State.accumulatedTimeMs);

        if (g_State.accumulatedTimeMs >= config.CoreConfig.coreDrainTickIntervalMs) {
            LOG(LogLevel::Verbose, "Core drain tick interval reached. Processing core simulation...");

            float hoursDelta = CalculateTimescaleDelta(currentTotalMinutes);

            if (hoursDelta == 0.0f) {
                // The real-world interval passed, but the in-game integer clock hasn't ticked up yet.
                // Do NOT reset accumulatedTimeMs. Let it retry on the next coreConfig.stateChangeTickIntervalMs loop pass.
                LOG(LogLevel::Verbose, "Interval boundary hit, but game engine clock hasn't rolled to a new integer minute. Holding accumulation cache.");
            }
            // If time actually moved forward in the engine space, process simulation and reset
            else if (hoursDelta > 0.0f && hoursDelta < 24.0f) {
                LOG(LogLevel::Standard, "Processing normal core simulation for elapsed hours: {}", hoursDelta);

                ProcessPlayerSimulation(playerPed, hoursDelta, context, config);
                ProcessHorseSimulation(playerPed, hoursDelta, context, config);

                g_MovementTracker.ResetSnapshot();

                // Only clear accumulation on successful simulation cycles!
                g_State.accumulatedTimeMs = 0;
            }
            // If the delta is negative, it indicates a macro time jump (e.g., train, stagecoach, or fast travel)
            else {
                LOG(LogLevel::Standard, "Macro time jump detected during active play (Train/Stagecoach). Triggering batch catch-up.");

                // We force a manual time skip evaluation using the global cache states
                // by temporarily simulating a transition block or a direct clean helper.
                ProcessOpenWorldTimeSkip(playerPed, currentTotalMinutes, config);

                g_State.accumulatedTimeMs = 0; // Clear the accumulation safely
            }

            // Update the last tick time to the current total minutes after processing
            LOG_DEV("Updating last tick in-game minutes from {} to {}", g_State.lastTickInGameMinutes, currentTotalMinutes);
            g_State.lastTickInGameMinutes = currentTotalMinutes;
        }
    }
}

void ScriptMain() {
    // ensure the background thread is cleanly terminated on script exit
	std::atexit([]() { g_ThreadManager.Shutdown(true); });

    LoadConfiguration();
    PrecomputeHashes();
    InitialiseActivityMonitors();
    
    // Start the INI watcher thread to monitor for configuration changes
    // Let the global manager handle spinning up the background watcher thread
    g_ThreadManager.Initialise();    

    LOG(LogLevel::ConfigOnly, "==== Core Drain Management Simulation Engine Initialised Successfully [{}] ====", VERSION);	
        
    while (true) {
        UpdateCoreSimulation();
        scriptWait(CalculateStateChangeTickIntervalMs());
    }
}