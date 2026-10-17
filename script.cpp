/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#define NOMINMAX

#include <string>
#include <atomic>
#include <mutex>
#include <ios>
#include <sstream>
#include <algorithm>
#include <tuple>
#include <unordered_map>
#include <cstdlib>
#include "script.h"
#include "Logger.h"
#include "Config.h"
#include "ThreadManager.h"
#include "CMTypes.h"
#include "Constants.h"
#include "TransientActivityMonitor.h"

enum class CoreIndex : int { Health = 0, Stamina = 1, DeadEye = 2 };
enum class HorseSpeed : int { Trot = 2, Gallop = 5 };

SimulationState g_State;

static CoreConfigSettings    s_cachedCoreConfig;
static GeneralConfigSettings s_cachedGeneralConfig;
static PlayerConfigSettings  s_cachedPlayerConfig;
static HorseConfigSettings   s_cachedHorseConfig;
static bool s_structuresInitialized = false;

std::unordered_map<Hash, std::string> g_SleepScenarios{};
std::unordered_map<Hash, std::string> g_CampScenarios{};
std::unordered_map<TransientActivityType, TransientActivityMonitor> g_activeMonitors{};

Hash g_PokerScriptHash;
Hash g_BlackjackScriptHash;
Hash g_DominoesScriptHash;
Hash g_FiveFingerFilletScriptHash;

Hash GetKey(const char* key) {
	Hash hash = MISC::GET_HASH_KEY(key);
	WriteLog(LogLevel::Dev, "Precomputed hash for key '" + std::string(key) + "': " + std::to_string(hash));
    
    return hash;
}

void PrecomputeHashes() {
	// Precompute hashes here to avoid repeated calls to MISC::GET_HASH_KEY during gameplay.

    g_SleepScenarios = {
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL"), "WORLD_PLAYER_SLEEP_BEDROLL" },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL_ARTHUR"), "WORLD_PLAYER_SLEEP_BEDROLL_ARTHUR" },
        { GetKey("WORLD_PLAYER_SLEEP_BEDROLL_JOHN"), "WORLD_PLAYER_SLEEP_BEDROLL_JOHN" },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND"), "WORLD_PLAYER_SLEEP_GROUND" },
		{ GetKey("PROP_PLAYER_SLEEP_BED"), "PROP_PLAYER_SLEEP_BED" }, // hotel room bed
        { GetKey("PROP_PLAYER_SLEEP_BED_ARTHUR"), "PROP_PLAYER_SLEEP_BED_ARTHUR" },
        { GetKey("PROP_PLAYER_SLEEP_BED_JOHN"), "PROP_PLAYER_SLEEP_BED_JOHN" },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME"), "PROP_PLAYER_SLEEP_TENT_A_FRAME" },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_ARTHUR"), "PROP_PLAYER_SLEEP_TENT_A_FRAME_ARTHUR" },
        { GetKey("PROP_CAMP_SLEEP_BEDROLL"), "PROP_CAMP_SLEEP_BEDROLL" },
        { GetKey("PROP_PLAYER_SLEEP_CAMP_BEDROLL"), "PROP_PLAYER_SLEEP_CAMP_BEDROLL" },
        { GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_JOHN"), "PROP_PLAYER_SLEEP_TENT_A_FRAME_JOHN" },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY"), "PROP_PLAYER_SLEEP_TENT_CANOPY" },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY_ARTHUR"), "PROP_PLAYER_SLEEP_TENT_CANOPY_ARTHUR" },
        { GetKey("PROP_PLAYER_SLEEP_TENT_CANOPY_JOHN"), "PROP_PLAYER_SLEEP_TENT_CANOPY_JOHN" },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND_ARTHUR"), "WORLD_PLAYER_SLEEP_GROUND_ARTHUR" },
        { GetKey("WORLD_PLAYER_SLEEP_GROUND_JOHN"), "WORLD_PLAYER_SLEEP_GROUND_JOHN" }
    };

    g_CampScenarios = {
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL1"), "WORLD_PLAYER_CAMP_FIRE_KNEEL1" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL2"), "WORLD_PLAYER_CAMP_FIRE_KNEEL2" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT"), "WORLD_PLAYER_CAMP_FIRE_SIT" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_GROUND"), "WORLD_PLAYER_CAMP_FIRE_SIT_GROUND" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SQUAT"), "WORLD_PLAYER_CAMP_FIRE_SQUAT" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_CRAFT"), "WORLD_PLAYER_CAMP_FIRE_CRAFT" },
        { GetKey("WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_ARTHUR"), "WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_ARTHUR" },
        { GetKey("WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_JOHN"), "WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_JOHN" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_ARTHUR"), "WORLD_PLAYER_CAMP_FIRE_SIT_ARTHUR" },
        { GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_JOHN"), "WORLD_PLAYER_CAMP_FIRE_SIT_JOHN" },
        { GetKey("WORLD_PLAYER_REST_GROUND"), "WORLD_PLAYER_REST_GROUND" },
        { GetKey("WORLD_PLAYER_REST_GROUND_ARTHUR"), "WORLD_PLAYER_REST_GROUND_ARTHUR" },
        { GetKey("WORLD_PLAYER_REST_GROUND_JOHN"), "WORLD_PLAYER_REST_GROUND_JOHN" },
        { GetKey("PROP_CAMP_FIRE_SIT_ARTHUR"), "PROP_CAMP_FIRE_SIT_ARTHUR" },
        { GetKey("PROP_CAMP_FIRE_SIT_JOHN"), "PROP_CAMP_FIRE_SIT_JOHN" },
        { GetKey("WORLD_PLAYER_DYNAMIC_KNEEL"), "WORLD_PLAYER_DYNAMIC_KNEEL" }
    };

    g_PokerScriptHash = GetKey("poker_core");
	g_BlackjackScriptHash = GetKey("blackjack_core");
	g_DominoesScriptHash = GetKey("dominoes_core");
	g_FiveFingerFilletScriptHash = GetKey("five_finger_fillet");

	// additional hashes can be precomputed here if needed
}

void InitialiseActivityMonitors() {
	g_activeMonitors.emplace(TransientActivityType::Bathing, TransientActivityMonitor(
		TransientActivityType::Bathing,
		20, // 20 seconds threshold for bathing reset
		[]() -> bool {
            Ped playerPed = PLAYER::PLAYER_PED_ID();
            std::string bathingStateName = TASK::GET_TASK_MOVE_NETWORK_STATE(playerPed);

			WriteLog(LogLevel::Dev, "Bathing state check: current bathing state = " + bathingStateName);

            return VALID_BATHING_STATES.contains(bathingStateName);
		}
	));
	g_activeMonitors.emplace(TransientActivityType::Jail, TransientActivityMonitor(
		TransientActivityType::Jail,
		22, // 22 seconds threshold for jail reset
		[]() -> bool {
            Player playerId = PLAYER::PLAYER_ID();
            bool isArrested = PLAYER::IS_PLAYER_BEING_ARRESTED(playerId, false);
            bool isInCustody = !PLAYER::IS_PLAYER_PLAYING(playerId);

            WriteLog(LogLevel::Dev, "IsPlayerBeingArrested check: isBeingArrested=" + std::string(isArrested ? "true" : "false") + ", isInCustody=" + std::string(isInCustody ? "true" : "false"));

			return (isArrested && isInCustody);
		}
	));
	// Additional monitors can be added here if needed
}

int GetTotalInGameMinutes() {
    return (CLOCK::GET_CLOCK_HOURS() * 60) + CLOCK::GET_CLOCK_MINUTES();
}

bool IsInActiveScenario(Ped playerPed, const std::unordered_map<Hash, std::string>& scenarioSet) {
    Hash activeScenario = TASK::_GET_SCENARIO_POINT_TYPE_PED_IS_USING(playerPed);
	WriteLog(LogLevel::Dev, "Active scenario hash: " + std::to_string(activeScenario));

    // Fall back to transient dynamic actions if the primary structural anchor returns empty
	if (activeScenario == 0) {
		activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);
		WriteLog(LogLevel::Dev, "Fallback to dynamic scenario hash: " + std::to_string(activeScenario));
	}

	if (activeScenario == 0) return false;

	// If the current log level is below Dev, we skip the detailed logging and just return the result.
    if (g_CoreConfig.currentLogLevel < LogLevel::Dev)
        return scenarioSet.contains(activeScenario);

    bool isInScenarioSet = (scenarioSet.contains(activeScenario));

	if (isInScenarioSet) {
		WriteLog(LogLevel::Dev, "Active scenario matches known set: " + scenarioSet.at(activeScenario));
	}
	else {
		WriteLog(LogLevel::Dev, "Active scenario does not match any known set.");
	}

	return isInScenarioSet;
}

bool IsInSleepScenario(Ped playerPed) {
	bool isInSleepScenario = IsInActiveScenario(playerPed, g_SleepScenarios);
	WriteLog(LogLevel::Dev, "IsInSleepScenario check: " + std::string(isInSleepScenario ? "true" : "false"));

	return isInSleepScenario;
}

bool IsInCampScenario(Ped playerPed) {
    bool isInCampScenario = IsInActiveScenario(playerPed, g_CampScenarios);
    WriteLog(LogLevel::Dev, "IsInCampScenario check: " + std::string(isInCampScenario ? "true" : "false"));

    return isInCampScenario;
}

bool IsPlayerArrested() {
    TransientActivityMonitor& jailMonitor = g_activeMonitors[TransientActivityType::Jail];

    if (jailMonitor.IsActive()) {
        jailMonitor.MonitorCheck();

        if (jailMonitor.IsExpired()) {
            jailMonitor.Reset();

            WriteLog(LogLevel::Dev, "Player jailed check: Player has exited jail. Resetting transient jailed state.");
            return false;
        }

        WriteLog(LogLevel::Dev, "Player jailed check: Player is still in jail");
        return true;
    }
    else {
        if(jailMonitor.ActivateMonitor()) {
            WriteLog(LogLevel::Dev, "Player is being arrested and is in custody.");
            return true;
        }

        WriteLog(LogLevel::Dev, "Player is not being arrested or is not in custody.");
        return false;
    }
}

bool IsPlayerPlayingTableMinigame() {

	int pokerThreads = SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_PokerScriptHash);
	int blackjackThreads = SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_BlackjackScriptHash);
	int dominoesThreads = SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_DominoesScriptHash);
	int fiveFingerFilletThreads = SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_FiveFingerFilletScriptHash); 

	WriteLog(LogLevel::Dev, "Table minigame thread counts - Poker: " + std::to_string(pokerThreads) + ", Blackjack: " + std::to_string(blackjackThreads) + ", Dominoes: " + std::to_string(dominoesThreads) + ", Five Finger Fillet: " + std::to_string(fiveFingerFilletThreads));

	return (pokerThreads > 0 || blackjackThreads > 0 || dominoesThreads > 0 || fiveFingerFilletThreads > 0);
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

            WriteLog(LogLevel::Dev, "Player bathing check: Player has exited the bath. Resetting transient bathing state.");
            return false;
        }

        WriteLog(LogLevel::Dev, "Player bathing check: Player is still in a bath");
        return true;
    }
    else {
        if (bathingMonitor.ActivateMonitor()) {
            WriteLog(LogLevel::Dev, "Player bathing check: Player is now in a bath.");
            return true;
        }

        WriteLog(LogLevel::Dev, "Player bathing check: Player is not in a bath.");
        return false;
    }
}

float GetHorseDistanceFromPlayer(Ped playerPed, Ped horsePed) {
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

[[nodiscard]] std::tuple<Ped, bool> GetActivePlayerHorse(Ped playerPed) {
    Ped activeHorse = PED::GET_MOUNT(playerPed);

    // Fall back to their primary saddled mount if they aren't actively riding
    if (activeHorse == 0)
        activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(PLAYER::PLAYER_ID());

    bool hasHorse = (activeHorse != 0 &&
        ENTITY::DOES_ENTITY_EXIST(activeHorse) &&
        !ENTITY::IS_ENTITY_DEAD(activeHorse));

    if (hasHorse) {
		float distance = GetHorseDistanceFromPlayer(playerPed, activeHorse);

		WriteLog(LogLevel::Dev, "Active horse detected. Distance from player: " + std::to_string(distance) + " metres.");

		// If the horse is too far away, we consider it "lost" and reset the state.
		// 400 metres is a generous distance, 350m is the Bonding Level 4 max whistle distance.
		// Deters exploiting the drain mechanics for the horse by leaving it in a distant location while the player is elsewhere.
        if (distance > 400.0f) {
			WriteLog(LogLevel::Verbose, "Active horse is too far away (" + std::to_string(distance) + " metres). Resetting horse state.");
            hasHorse = false;
            activeHorse = 0;
        }
    }

    return { activeHorse, hasHorse };
}

void UpdateHorseHitchedCheck(GameplayContext ctx, Ped playerPed)
{
    auto [activeHorse, hasHorse] = GetActivePlayerHorse(playerPed);

    // Hard reset the accumulator as we no longer have a horse or we're in a freeze state
	if (!hasHorse || ctx.freezeActive) {
		WriteLog(LogLevel::Verbose, "Horse hitched check: No active horse or freeze state detected. Resetting accumulated immobility counter.");
        
        g_State.accumulatedHorseHitchedImmobility = 0;
        g_State.wasHorseHitchedLastTick = false;
        
        return;
    }

    // INSTANT-PASS ENGINE MEMORY FLAGS
    // If internal engine bits confirm a hitch or physical rope constraint, skip accumulation entirely.
    if (PED::GET_PED_CONFIG_FLAG(activeHorse, 217, true) ||
        PED::GET_PED_CONFIG_FLAG(activeHorse, 287, true) ||
        invoke<bool>(0xDFE2C6AA327663E7, activeHorse)) // _IS_PED_CONSTRAINED
    {
		WriteLog(LogLevel::Verbose, "Horse hitched check: Engine memory flags indicate horse is physically hitched. Setting state to hitched.");
        
        g_State.accumulatedHorseHitchedImmobility = HORSE_HITCHED_REQ;
        g_State.wasHorseHitchedLastTick = true;
        return;
    }

    // HIGH-PRECISION VELOCITY & SCENARIO FILTER
    float horseSpeed = ENTITY::GET_ENTITY_SPEED(activeHorse);

    // Condition: True zero velocity
    if (horseSpeed == 0.0f)
    {
        bool isBeingRidden = (PED::GET_MOUNT(playerPed) == activeHorse);

        if (!isBeingRidden)
        {
			// If the horse is stationary, not being ridden and not confirmed hitched already, we increment the immobility counter. 
            // Once it reaches the threshold, we consider the horse hitched.
			if (!g_State.wasHorseHitchedLastTick) {
                WriteLog(LogLevel::Verbose, "Horse hitched check: Horse is stationary and not being ridden. Accumulating immobility counter.");
                g_State.accumulatedHorseHitchedImmobility++;

				if (g_State.accumulatedHorseHitchedImmobility >= HORSE_HITCHED_REQ) // default threshold: 6 ticks of 500ms = 3 seconds of immobility
                {
                    WriteLog(LogLevel::Verbose, "Horse hitched check: Accumulated immobility threshold reached. Horse is now considered hitched.");
                    g_State.accumulatedHorseHitchedImmobility = HORSE_HITCHED_REQ;
                    g_State.wasHorseHitchedLastTick = true;
                }
            }

            return;
        }
    }

    // RAPID RESET
    // If any check breaks for even a single 500ms loop tick, immediately drop the state
	WriteLog(LogLevel::Verbose, "Horse hitched check: Rapid reset triggered. Horse is not hitched. Resetting accumulated immobility counter and hitched state.");
    g_State.accumulatedHorseHitchedImmobility = 0;
    g_State.wasHorseHitchedLastTick = false;
}

GameplayContext EvaluateGameplayState(Ped playerPed, const GeneralConfigSettings& generalConfig) {
    GameplayContext ctx;

    bool isInMission = MISC::GET_MISSION_FLAG();
    bool isInMinigame = IsPlayerPlayingTableMinigame();
    bool isFallingOrGoingToDie = PED::IS_PED_FALLING(playerPed) || GRAPHICS::ANIMPOSTFX_IS_RUNNING("MissionFail01");
    
    ctx.isBathing = IsPlayerBathing(playerPed);
    ctx.isJailed = IsPlayerArrested();
	ctx.isSleeping = IsInSleepScenario(playerPed);
	ctx.isAtCamp = IsInCampScenario(playerPed);
    
    // If the player is in a mission or minigame and the config disallows drain, we freeze the decay.
    ctx.isInMission = isInMission && !generalConfig.enableDuringMissions;
    ctx.isInMinigame = isInMinigame && !generalConfig.enableDuringMinigames;

    // camp stays out as we want a customized slowed decay in those cases
    ctx.freezeActive = (ctx.isInMission || ctx.isInMinigame || isFallingOrGoingToDie || ctx.isSleeping || ctx.isBathing || ctx.isJailed);
    ctx.isFreezeVariable = ctx.isBathing; // Bathing is treated as a variable/semi freeze as we need to track deluxe upgrade across the next period of ticks
    
    UpdateHorseHitchedCheck(ctx, playerPed);

	if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev) && HasDevLogCacheChanged(ctx)) {
        WriteLog(LogLevel::Dev, "Gameplay state evaluated: allowDrainInMissions=" + std::to_string(generalConfig.enableDuringMissions) + ", allowDrainInMinigames=" + std::to_string(generalConfig.enableDuringMinigames));

        // Update tracking baselines immediately
        UpdateDevLogCache(ctx);

        std::ostringstream message;
        message << std::boolalpha
            << "Gameplay state changed:"
            << " isInMission = " << ctx.isInMission
            << ", isInMinigame=" << ctx.isInMinigame
            << ", isFallingOrGoingToDie=" << isFallingOrGoingToDie
            << ", isBathing=" << ctx.isBathing
            << ", isSleeping=" << ctx.isSleeping
            << ", isAtCamp=" << ctx.isAtCamp
            << ", isJailed=" << ctx.isJailed
            << ", isFreezeActive=" << ctx.freezeActive;

        WriteLog(LogLevel::Dev, message.str());
    }    

    return ctx;
}

float CalculatePlayerStaminaSleepReplenish(float currentStamina, float elapsedHours,bool isNighttime, bool isHotelRoom) {
    // Standard linear scaling: 10 points per hour of sleep
    float uplift = elapsedHours * 10.0f;

    // POOR SLEEP PENALTY (Less than 8 Hours or daytime and in the wilderness)
	// If you don't get the minimum of rest, your maximum reward capacity is capped hard at 40 points (20 if it's daytime and you're not in a hotel room).
    if (elapsedHours < MIN_SLEEP_HOURS) {
        float cap = (isNighttime || isHotelRoom) ? 40.0f : 20.0f;
        uplift = std::min(uplift, cap);
        WriteLog(LogLevel::Verbose, "Short sleep session detected (< " + std::to_string(MIN_SLEEP_HOURS) + " hours). Restorative stamina uplift capped at 40 points.");
    }

	// DAYTIME WILDERNESS SLEEP PENALTY (Minimum Hours but not in a hotel room)
	// If you sleep for the minimum or more hours but it's daytime and you're not in a hotel room, your maximum reward capacity is capped hard at 50 points.
	if (elapsedHours >= MIN_SLEEP_HOURS && (!isNighttime && !isHotelRoom)) {
        uplift = std::min(uplift, 50.0f);
		WriteLog(LogLevel::Verbose, "Daytime wilderness sleep detected. Stamina uplift capped at 50 points.");
	}

    float finalStamina = currentStamina + uplift;
    return std::min(finalStamina, 100.0f); // Standard full core ceiling
}

float CalculatePlayerDeadEyeSleepReplenish(float currentDeadEye, float elapsedHours, bool isNighttime, bool isHotelRoom) {
    // Establish the Baseline Focus Threshold Caps

    float focusCap = 60.0f;   // Maximum normal sleep reward boundary
    float fatigueCap = 50.0f; // Maximum over-sleep unfocused boundary

    // Luxury Hotel Quality Adjustment (+20/+10 points to caps)
    if (isHotelRoom) {
        if (isNighttime) {
			WriteLog(LogLevel::Verbose, "Luxury hotel room detected at night. Dead Eye caps increased by +20 points.");
            focusCap += 20.0f;
            fatigueCap += 20.0f;
		}
        else {
            WriteLog(LogLevel::Verbose, "Luxury hotel room detected during daytime. Dead Eye caps increased by +10 points.");
            focusCap += 10.0f;
            fatigueCap += 10.0f;
        }
    }

	// Daytime Wilderness Penalty (-5 points to caps)
    if (!isNighttime && !isHotelRoom) {
		WriteLog(LogLevel::Verbose, "Daytime wilderness sleep detected. Dead Eye caps decreased by -5 points.");
        focusCap -= 5.0f;
        fatigueCap -= 5.0f;
    }

    // THE OVER-SLEEP UNFOCUS ROUTE (OverSleep Threshold Hours)
    if (elapsedHours >= OVERSLEEP_THRESHOLD_HOURS) {
		// If current dead eye is higher than the fatigue boundary, apply the 15-point reduction penalty (5 points if in a hotel room)
        if (currentDeadEye > fatigueCap) {
			WriteLog(LogLevel::Verbose, "Over-sleep detected. Dead Eye reduced by 15 points (5 if in a hotel room).");
            return std::max(currentDeadEye - (isHotelRoom ? 5.0f : 15.0f), fatigueCap);
        }

        return fatigueCap;
    }

    // THE REGULAR SLEEP ROUTE (Less than OverSleep Threshold Hours)
    // If your starting Dead Eye is already higher than the focus cap, leave it completely untouched!
    if (currentDeadEye >= focusCap)
        return currentDeadEye;

    float uplift = elapsedHours * 5.0f;
    float finalDeadEye = currentDeadEye + uplift;

    return std::min(finalDeadEye, focusCap);
}

float CalculateHorseStaminaSleepReplenish(float currentStamina, float elapsedHours, bool isNighttime, bool isHotelRoom) {
    // Standard linear scaling: 10 points per hour of sleep
    float uplift = elapsedHours * 10.0f;

    // DAYTIME WILDERNESS SLEEP PENALTY (not in a stable (hotel) room at that)
    // If you sleep for the minimum or more hours but it's daytime and you're not in a hotel room, the horse's maximum reward capacity is capped hard at 50 points.
    if (!isNighttime && !isHotelRoom) {
        uplift = std::min(uplift, 50.0f);
        WriteLog(LogLevel::Verbose, "Daytime wilderness sleep detected. Horse Stamina uplift capped at 50 points.");
    }

    float finalStamina = currentStamina + uplift;
    return std::min(finalStamina, 100.0f); // Standard full core ceiling
}

void ApplyBathingUpdate(Ped playerPed, int elapsedMinutes, float startHealth, float startStamina, float startDeadEye,
    const PlayerConfigSettings& playerConfig) {

    float elapsedHours = static_cast<float>(elapsedMinutes) / 60.0f;

    elapsedHours = std::min(elapsedHours, BATHING_TIMESKIP_MAX_HOURS);

    bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
    bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
    bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    if (!isHealthGold) {
        int finalHealth = static_cast<int>(std::max(startHealth - (elapsedHours * playerConfig.baseHealthDecay),
            playerConfig.campedHealthFloor));

        WriteLog(LogLevel::Standard, "Player time-skipped. Health decayed to: " + std::to_string(finalHealth));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

    // Freeze standard drain and apply custom uplift reward
    if (!isStaminaGold) {
        float staminaUplift = 15.0f; // Flat 15 point bonus

        if (g_State.cachedBathingState == 2) {
            WriteLog(LogLevel::Verbose, "Player Bathing Deluxe Service: Stamina bonus doubled!!");
            staminaUplift = 30.0f; // Double bonus for Deluxe Service (Maid)
        }
        else {
			WriteLog(LogLevel::Verbose, "Player Bathing Standard Service: Standard Stamina bonus.");
        }

        float targetStamina = std::min(startStamina + staminaUplift, 100.0f);

        WriteLog(LogLevel::Verbose, "Bathing Stamina Uplift Reward Applied: " + std::to_string(targetStamina));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
    }

    // Freeze standard drain and apply custom bath metrics
    if (!isDeadEyeGold) {
        // SPECIAL GOLDEN DEADEYE CORE REWARD (Bathing Deluxe Service (Maid))
        if (g_State.cachedBathingState == 2) {
            WriteLog(LogLevel::Verbose, "Player Bathing Deluxe Service: Dead Eye is now golden and fully fortified.");
            ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(playerPed, static_cast<int>(CoreIndex::DeadEye), 100.0f, true);
        }
        else {
			float targetDeadEye = startDeadEye;

            if (startDeadEye < 80.0f) {
                // Calculate the uplift per hour based on the maximum cap of 80.0f over the BATHING_TIMESKIP_MAX_HOURS
				float upliftPerHour = 80.0f / BATHING_TIMESKIP_MAX_HOURS; 

                targetDeadEye = std::min(startDeadEye + (elapsedHours * upliftPerHour), 80.0f);
                WriteLog(LogLevel::Verbose, "Bathing Standard Service: Dead Eye capped at a maximum baseline of 80.0f.");
            }
            else {
                WriteLog(LogLevel::Verbose, "Bathing Standard Service: Dead Eye already at or above 80.0f. No uplift applied.");
            }

            WriteLog(LogLevel::Standard, "Player DeadEye Sync Tick: DeadEye=" + std::to_string(targetDeadEye));
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), static_cast<int>(targetDeadEye));
        }
    }
}

void ApplyBatchTimeSkipDecay(
	Ped playerPed, int elapsedMinutes, float startHealth, float startStamina, float startDeadEye, 
    float startHorseHealth, float startHorseStamina, bool processHorse,
	const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig,
    bool wasSleeping = false, bool wasInMinigame = false) {

    float elapsedHours = static_cast<float>(elapsedMinutes) / 60.0f;

    if (wasInMinigame) {
		WriteLog(LogLevel::Verbose, "Player is in a minigame. Capping time skip to MINIGAME_TIMESKIP_MAX_HOURS hours for core decay calculations.");
		elapsedHours = std::min(elapsedHours, MINIGAME_TIMESKIP_MAX_HOURS); // Cap the maximum time skip to 3 hours for minigames
    }
    else if (elapsedHours > TIMESKIP_MAX_HOURS) {
		WriteLog(LogLevel::Verbose, "Time skip exceeds TIMESKIP_MAX_HOURS hours. Capping to TIMESKIP_MAX_HOURS hours for core decay calculations.");
        elapsedHours = TIMESKIP_MAX_HOURS;
    }

    bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
    bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
    bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

	// Health Core Decay Logic
    if (!isHealthGold) {
        int finalHealth = static_cast<int>(std::max(startHealth - (elapsedHours * playerConfig.baseHealthDecay),
            wasSleeping ? playerConfig.healthTimeSkipFloor : playerConfig.campedHealthFloor));

		WriteLog(LogLevel::Standard, "Player time-skipped. Health decayed to: " + std::to_string(finalHealth));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

	// Stamina Core Replenishment Logic
    if (wasSleeping) {
        // Gold cores are immune to sleep cap constraints, they remain full
        if (!isStaminaGold) {
            int currentHour = CLOCK::GET_CLOCK_HOURS();
            bool isNighttime = (currentHour >= generalConfig.nightStartHour || currentHour < generalConfig.nightEndHour);
			bool isHotelRoom = !g_State.wasAtCampDuringRestriction;

			// SPECIAL GOLDEN STAMINA CORE REWARD (Minimum Sleep Hours-OverSleep Threshold Hours Hours in a Hotel Room at Night)
            if (isHotelRoom && isNighttime && elapsedHours >= MIN_SLEEP_HOURS && elapsedHours <= OVERSLEEP_THRESHOLD_HOURS) {
				
				WriteLog(LogLevel::Standard, "Player slept " + std::to_string(MIN_SLEEP_HOURS) + "-" + std::to_string(OVERSLEEP_THRESHOLD_HOURS) + " hours in a hotel room at night. Stamina core is now golden and fully fortified.");
				ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(playerPed, static_cast<int>(CoreIndex::Stamina), 100.0f, true);
            }
			// REGULAR SLEEP REPLENISHMENT ROUTE (< Minimum Sleep Hours or > OverSleep Threshold Hours and NOT in a Hotel Room at Night)
            else {

                int finalStamina = static_cast<int>(CalculatePlayerStaminaSleepReplenish(startStamina, elapsedHours, isNighttime, isHotelRoom));

                WriteLog(LogLevel::Standard, "Player slept. Stamina replenished to: " + std::to_string(finalStamina));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
            }
        }
    }
    else if (!isStaminaGold) {
        // Standard macro transit decay (Trains/Stagecoaches/Jail/Barber)
        int finalStamina = static_cast<int>(std::max(startStamina - (elapsedHours * playerConfig.baseStaminaDecay), playerConfig.campedStaminaFloor));

		WriteLog(LogLevel::Standard, "Player time-skipped. Stamina decayed to: " + std::to_string(finalStamina));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
    }

	// Dead Eye Core Replenishment Logic
    if (wasSleeping) {
        int currentHour = CLOCK::GET_CLOCK_HOURS();
        bool isNighttime = (currentHour >= generalConfig.nightStartHour || currentHour < generalConfig.nightEndHour);
        bool isHotelRoom = !g_State.wasAtCampDuringRestriction;

        if (!isDeadEyeGold) {
            // Standard sleep replenishment only runs if the core isn't already golden
            int finalDeadEye = static_cast<int>(CalculatePlayerDeadEyeSleepReplenish(startDeadEye, elapsedHours, isNighttime, isHotelRoom));
            
            WriteLog(LogLevel::Standard, "Player slept. Dead Eye adjusted to: " + std::to_string(finalDeadEye));
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
        }
    }
    else if (!isDeadEyeGold) {
        // Standard macro transit decay (Trains/Stagecoaches/Jail/Barber)
        int finalDeadEye = static_cast<int>(std::max(startDeadEye - (elapsedHours * playerConfig.baseDeadEyeDecay), playerConfig.campedDeadEyeFloor));

		WriteLog(LogLevel::Standard, "Player time-skipped. Dead Eye decayed to: " + std::to_string(finalDeadEye));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
    }

    // Compute Horse Core Reductions

	if (processHorse) {
        auto [activeHorse, hasHorse] = GetActivePlayerHorse(playerPed);
        if (hasHorse) {
            bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(activeHorse, static_cast<int>(CoreIndex::Health));
            bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(activeHorse, static_cast<int>(CoreIndex::Stamina));

			// Horse Health Core Decay Logic
            if (!isHorseHealthGold) {
                int finalHorseHealth = static_cast<int>(std::max(startHorseHealth - (elapsedHours * horseConfig.baseHealthDecay), wasSleeping ? horseConfig.healthTimeSkipFloor : horseConfig.campedHealthFloor));
                WriteLog(LogLevel::Standard, "Player time-skipped. Horse Health decayed to: " + std::to_string(finalHorseHealth));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health), finalHorseHealth);
            }

			// Horse Stamina Core Decay Logic
            if (wasSleeping) {
                // Gold cores are immune to sleep cap constraints, they remain full
                if (!isHorseStaminaGold) {
                    int currentHour = CLOCK::GET_CLOCK_HOURS();
                    bool isNighttime = (currentHour >= generalConfig.nightStartHour || currentHour < generalConfig.nightEndHour);
                    bool isHotelRoom = !g_State.wasAtCampDuringRestriction;

                    // SPECIAL GOLDEN STAMINA CORE REWARD (Hotel Stable at Night)
                    if (isHotelRoom && isNighttime) {

                        WriteLog(LogLevel::Standard, "Horse slept in a stable (hotel) at night. Stamina core is now golden and fully fortified.");
                        ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(activeHorse, static_cast<int>(CoreIndex::Stamina), 100.0f, true);
                    }
                    // REGULAR SLEEP REPLENISHMENT ROUTE
                    else {

                        int finalHorseStamina = static_cast<int>(CalculateHorseStaminaSleepReplenish(startStamina, elapsedHours, isNighttime, isHotelRoom));

                        WriteLog(LogLevel::Standard, "Player slept. Horse Stamina replenished to: " + std::to_string(finalHorseStamina));
                        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), finalHorseStamina);
                    }
                }
            }
            else if (!isStaminaGold) {
                // Standard macro transit decay (Trains/Stagecoaches)
                int finalHorseStamina = static_cast<int>(std::max(startHorseStamina - (elapsedHours * horseConfig.baseStaminaDecay), horseConfig.campedStaminaFloor));

                WriteLog(LogLevel::Standard, "Player time-skipped. Horse Stamina decayed to: " + std::to_string(finalHorseStamina));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), finalHorseStamina);
            }
        }
    }
}

void UpdateFrozenStateVariables() {
	WriteLog(LogLevel::Dev, "Updating frozen state variables based on cached values.");


	WriteLog(LogLevel::Dev, "Frozen state variables updated: isRestricted=" + std::to_string(g_State.isRestricted) +
		", isRestrictedVariable=" + std::to_string(g_State.isRestrictedVariable) +
		", cachedInGameTimeMinutes=" + std::to_string(g_State.cachedInGameTimeMinutes) +
		", lastTickInGameMinutes=" + std::to_string(g_State.lastTickInGameMinutes) +
		", wasSleepingDuringRestriction=" + std::to_string(g_State.wasSleepingDuringRestriction) +
		", wasAtCampDuringRestriction=" + std::to_string(g_State.wasAtCampDuringRestriction) +
		", wasInMissionDuringRestriction=" + std::to_string(g_State.wasInMissionDuringRestriction) +
		", wasInMinigameDuringRestriction=" + std::to_string(g_State.wasInMinigameDuringRestriction) +
		", isBathingDuringRestriction=" + std::to_string(g_State.isBathingDuringRestriction) +
		", cachedHealth=" + std::to_string(g_State.cachedHealth) +
		", cachedStamina=" + std::to_string(g_State.cachedStamina) +
		", cachedDeadEye=" + std::to_string(g_State.cachedDeadEye) +
		", cachedMoney=" + std::to_string(g_State.cachedMoney) +
        ", cachedBathingState=" + std::to_string(g_State.cachedBathingState) +
		", hadHorse=" + std::to_string(g_State.hadHorse) +
		", cachedHorseHealth=" + std::to_string(g_State.cachedHorseHealth) +
		", cachedHorseStamina=" + std::to_string(g_State.cachedHorseStamina));

	// Bathing check: If the player was bathing during the restriction period, we need to determine if they paid for a premium bath and adjust the cached bathing state accordingly.
	if (g_State.isBathingDuringRestriction) {
        if (g_State.cachedMoney > MONEY::_MONEY_GET_CASH_BALANCE() && g_State.cachedBathingState == 1) {
            WriteLog(LogLevel::Dev, "Player bathing check: Player has paid for a premium bath. Setting bathing state to Premium Bathing.");
            g_State.cachedBathingState = 2; // Premium Bathing
        }
	}

	// Add more frozen state variable updates here as needed in the future
}

void HandleStateTransitions(Ped playerPed, const GameplayContext& ctx, int currentTotalMinutes, const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {

    if (ctx.freezeActive && !g_State.isRestricted) {
        g_State.isRestricted = true;
		g_State.isRestrictedVariable = ctx.isFreezeVariable;

        g_State.cachedInGameTimeMinutes = currentTotalMinutes;
        g_State.lastTickInGameMinutes = g_State.cachedInGameTimeMinutes;

        // Lock the context down safely right here!
        g_State.wasSleepingDuringRestriction = ctx.isSleeping;
        g_State.wasAtCampDuringRestriction = ctx.isAtCamp;
		g_State.wasInMissionDuringRestriction = ctx.isInMission;
		g_State.wasInMinigameDuringRestriction = ctx.isInMinigame || ctx.isJailed;
		g_State.isBathingDuringRestriction = ctx.isBathing;
		g_State.cachedBathingState = ctx.isBathing ? 1 : 0; // Default to standard bathing state if we're in a bathing scenario, otherwise 0 (not bathing)

        // Cache player base structures
        g_State.cachedHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
        g_State.cachedStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
        g_State.cachedDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));
		g_State.cachedMoney = MONEY::_MONEY_GET_CASH_BALANCE();

		// Cache horse base structures if the player is mounted or we have an active mount in the world
        auto [activeHorse, hasHorse] = GetActivePlayerHorse(playerPed);
        if (hasHorse) {
            g_State.hadHorse = true;
            g_State.cachedHorseHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health)));
            g_State.cachedHorseStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina)));
            
            WriteLog(LogLevel::Verbose, "Entering restricted state. Cached player data and active horse tracking metrics remotely.");
        }
        else {
            g_State.hadHorse = false;
            g_State.cachedHorseHealth = 0.0f;
            g_State.cachedHorseStamina = 0.0f;
            
            WriteLog(LogLevel::Verbose, "Entering restricted state. Cached player data (No active world mount detected).");
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
		g_State.isRestrictedVariable = false;

        int elapsedMinutes = currentTotalMinutes - g_State.cachedInGameTimeMinutes;

        if (elapsedMinutes < 0)
            elapsedMinutes += 1440; // Self-corrects midnight rollover cleanly

        WriteLog(LogLevel::Verbose, "Exiting restricted state. Total elapsed game minutes: " + std::to_string(elapsedMinutes));

        if (elapsedMinutes > 0)
			// Mini-games are table mini-games/jail
            if (g_State.wasSleepingDuringRestriction || g_State.wasInMinigameDuringRestriction) {
                // Uses the HISTORICAL frozen values from the cache
				WriteLog(LogLevel::Verbose, "Player was sleeping/in a minigame/jailed during restriction. Applying batch time skip decay.");

                ApplyBatchTimeSkipDecay(playerPed, elapsedMinutes,
                    g_State.cachedHealth, g_State.cachedStamina, g_State.cachedDeadEye,
                    g_State.cachedHorseHealth, g_State.cachedHorseStamina, (g_State.hadHorse && !g_State.wasInMinigameDuringRestriction),
                    generalConfig, playerConfig, horseConfig,
                    g_State.wasSleepingDuringRestriction, g_State.wasInMinigameDuringRestriction);                
            }
            else if (g_State.isBathingDuringRestriction) {
                // Uses the HISTORICAL frozen values from the cache
                WriteLog(LogLevel::Verbose, "Player was bathing during restriction. Applying batch time skip decay with bathing multiplier.");

                ApplyBathingUpdate(playerPed, elapsedMinutes,
                    g_State.cachedHealth, g_State.cachedStamina, g_State.cachedDeadEye,
                    playerConfig);
            }
            else if (g_State.wasInMissionDuringRestriction)
            {
                g_State.PostMissionDecayHoursRemaining += POST_MISSION_DECAY_HOURS;

				// Cap the post-mission decay hours to a maximum of 3 hours to prevent excessive decay 
                // (in-case multiple missions accumulate over the decay multiplier period)
				g_State.PostMissionDecayHoursRemaining = std::min(g_State.PostMissionDecayHoursRemaining, 3.0f);
				WriteLog(LogLevel::Verbose, "Player exited a mission. Post-mission decay hours remaining: " + std::to_string(g_State.PostMissionDecayHoursRemaining));
            }
            else {
				WriteLog(LogLevel::Dev, "Player was not sleeping, in a minigame, or in a mission during restriction. No batch decay applied.");
            }

        g_State.wasSleepingDuringRestriction = false;
        g_State.wasInMinigameDuringRestriction = false;
		g_State.wasInMissionDuringRestriction = false;
        g_State.isBathingDuringRestriction = false;

		// Reset the cached bathing state and money to prevent false positives on future bathing checks
		g_State.cachedBathingState = 0;
        g_State.cachedMoney = 0;

        // This stops CalculateTimescaleDelta from processing a phantom macro-jump on the next loop tick.
        g_State.cachedInGameTimeMinutes = currentTotalMinutes;
        g_State.lastTickInGameMinutes = currentTotalMinutes;
    }
}

float CalculateTimescaleDelta(int currentTotalMinutes)
{
    // Safe Initialization (Fails fast on frame 1 using the unified structure state)
    if (!g_State.timescaleDeltaInitialised) {
        g_State.lastTickInGameMinutes = currentTotalMinutes;
        g_State.timescaleDeltaInitialised = true;

        WriteLog(LogLevel::Dev, "Timescale delta initialized. Baseline in-game minutes: " + std::to_string(g_State.lastTickInGameMinutes));
        return 0.0f;
    }

    // Fail Fast Optimization
    if (currentTotalMinutes == g_State.lastTickInGameMinutes) return 0.0f;

    int gameMinutesDelta = currentTotalMinutes - g_State.lastTickInGameMinutes;

    // Midnight Rollover Correction
    // If the clock rolled backwards (e.g., from 1439 down to 5 mins), 
    // adding 1440 perfectly restores the true forward time delta.
    if (gameMinutesDelta < 0) {
        WriteLog(LogLevel::Dev, "Midnight rollover detected. Adjusting game minutes delta from " + std::to_string(gameMinutesDelta) + " to " + std::to_string(gameMinutesDelta + 1440));
        gameMinutesDelta += 1440;
    }

    // Latch the time baseline immediately into global tracking
    g_State.lastTickInGameMinutes = currentTotalMinutes;

    // UNMANAGED MACRO JUMP INTERCEPT (Trains, Coaches, Fast Travel)
    // If a time skip > 2 hours happens while the player has normal open-world agency,
    // we return a clear invalid value to signal that UpdateCoreSimulation should take over and process the jump.
    if (gameMinutesDelta > 120) {
        WriteLog(LogLevel::Dev, "Large time skip detected. Returning invalid value.");
        return -1.0f;
    }

    return static_cast<float>(gameMinutesDelta) / 60.0f;
}

bool IsPlayerMounted(Ped playerPed) {
    Ped activeHorse = PED::GET_MOUNT(playerPed);

    return (activeHorse != 0);
}

float GetPlayerMountedStaminaMultiplier(Ped playerPed) {
    Ped activeHorse = PED::GET_MOUNT(playerPed);

    if (activeHorse == 0)
        return 1.0f; // not on a horse

    float currentSpeed = ENTITY::GET_ENTITY_SPEED(activeHorse);

    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
		WriteLog(LogLevel::Dev, "Active horse is galloping. Applying Player stamina decay multiplier: 1.2");
        return 1.2f;
    }        
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot) && currentSpeed <= static_cast<float>(HorseSpeed::Gallop)) {
    	WriteLog(LogLevel::Dev, "Active horse is trotting. Applying Player stamina decay multiplier: 1.0");
        return 1.0f;
    }
    else {
		WriteLog(LogLevel::Dev, "Active horse is walking or stationary. Applying Player stamina decay multiplier: 0.8");
        return 0.8f;
    }
}

float CalculatePlayerHealthDecayMultiplier(bool isPlayerSemiRestrained, bool applyPostMissionDecay, const PlayerConfigSettings& playerConfig) {
	float multiplier = 1.0f;
    if (isPlayerSemiRestrained) {
        WriteLog(LogLevel::Dev, "Player is semi-restrained. Applying restrained health decay multiplier.");
        multiplier += playerConfig.campedMultiplier;
    }

    if (applyPostMissionDecay) {
        WriteLog(LogLevel::Dev, "Player is exiting a mission. Applying post-mission health decay multiplier.");
        multiplier += POST_MISSION_DECAY_MULTIPLIER;
    }

	return multiplier;
}

float CalculatePlayerStaminaDecayMultiplier(bool isPlayerSemiRestrained, bool applyPostMissionDecay, const PlayerConfigSettings& playerConfig, Ped playerPed) {
    float multiplier = 1.0f;
    if (isPlayerSemiRestrained) {
        WriteLog(LogLevel::Dev, "Player is semi-restrained. Applying restrained stamina decay multiplier.");
        multiplier += playerConfig.campedMultiplier;
    }
    else if (IsPlayerMounted(playerPed)) {
        float mountedStaminaMultiplier = GetPlayerMountedStaminaMultiplier(playerPed);
        WriteLog(LogLevel::Dev, "Player is mounted. Applying conditional mounted stamina decay multiplier: " + std::to_string(mountedStaminaMultiplier));
        multiplier += mountedStaminaMultiplier;
    }

    if (applyPostMissionDecay) {
        WriteLog(LogLevel::Dev, "Player is exiting a mission. Applying post-mission stamina decay multiplier.");
        multiplier += POST_MISSION_DECAY_MULTIPLIER;
    }

    return multiplier;
}

float CalculatePlayerDeadEyeDecayMultiplier(bool isPlayerSemiRestrained, bool applyPostMissionDecay, bool isNighttime, const PlayerConfigSettings& playerConfig) {
    float multiplier = 1.0f;
    if (isPlayerSemiRestrained) {
		WriteLog(LogLevel::Dev, "Player is semi-restrained. Applying restrained dead eye decay multiplier.");
        multiplier += playerConfig.campedMultiplier;
    }
    else if (isNighttime) {
		WriteLog(LogLevel::Dev, "Player is in nighttime. Applying night dead eye decay multiplier.");
        multiplier += playerConfig.nightDeadEyeMultiplier;
    }

    if (applyPostMissionDecay) {
        WriteLog(LogLevel::Dev, "Player is exiting a mission. Applying post-mission dead eye decay multiplier.");
        multiplier += POST_MISSION_DECAY_MULTIPLIER;
    }

    return multiplier;
}

void ProcessPlayerSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig) {
    int currentHour = CLOCK::GET_CLOCK_HOURS();
    bool isNighttime = (currentHour >= generalConfig.nightStartHour || currentHour < generalConfig.nightEndHour);

	bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
	bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
	bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    float currentHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
    float currentStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
    float currentDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));

    WriteLog(LogLevel::Verbose, "Player Core Simulation: isHealthGold = " + std::to_string(isHealthGold) + ", isStaminaGold = " + std::to_string(isStaminaGold) + ", isDeadEyeGold = " + std::to_string(isDeadEyeGold));

	// Track post-mission decay hours remaining if the player is exiting a mission. This will apply a multiplier to all cores for a limited time after the mission ends.
    bool applyPostMissionDecay = (g_State.PostMissionDecayHoursRemaining > 0.0f);

    if (applyPostMissionDecay) {
        g_State.PostMissionDecayHoursRemaining -= hoursDelta;
		WriteLog(LogLevel::Dev, "Post-mission decay is active. Remaining hours: " + std::to_string(g_State.PostMissionDecayHoursRemaining));

		// Ensure that the remaining hours do not go below zero
		g_State.PostMissionDecayHoursRemaining = std::max(g_State.PostMissionDecayHoursRemaining, 0.0f);
	}

    if (!isHealthGold) {
        float activeHealthDecay = playerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(ctx.isAtCamp, applyPostMissionDecay, playerConfig);

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);

        // The player is semi restrained (jail/camp), so we apply the player's restrained floor to prevent full depletion
		float minHealthFloor = ctx.isAtCamp ? playerConfig.campedHealthFloor : 0.0f;          
        targetHealth = std::max(targetHealth, minHealthFloor);

		WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Player Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), static_cast<int>(targetHealth));
    }

	if (!isStaminaGold) {
        float activeStaminaDecay = playerConfig.baseStaminaDecay * CalculatePlayerStaminaDecayMultiplier(ctx.isAtCamp, applyPostMissionDecay, playerConfig, playerPed);            

        float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
        float minStaminaFloor = ctx.isAtCamp ? playerConfig.campedStaminaFloor : 0.0f;
        targetStamina = std::max(targetStamina, minStaminaFloor);
            
        WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay = " + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor));
        
        WriteLog(LogLevel::Standard, "Player Stamina Sync Tick: Stamina=" + std::to_string(targetStamina));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
    }

    if (!isDeadEyeGold) {
        // Normal Open-World, Nighttime, or Semi-Restrained Decay Route
        float activeDeadEyeDecay = playerConfig.baseDeadEyeDecay * CalculatePlayerDeadEyeDecayMultiplier(ctx.isAtCamp, applyPostMissionDecay, isNighttime, playerConfig);

        float targetDeadEye = currentDeadEye - (activeDeadEyeDecay * hoursDelta);
        float minDeadEyeFloor = ctx.isAtCamp ? playerConfig.campedDeadEyeFloor : 0.0f;

        targetDeadEye = std::max(targetDeadEye, minDeadEyeFloor);

        WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", DeadEyeDecay = " + std::to_string(activeDeadEyeDecay) + ", TargetDeadEye=" + std::to_string(targetDeadEye) + ", MinDeadEyeFloor=" + std::to_string(minDeadEyeFloor));
            
        WriteLog(LogLevel::Standard, "Player DeadEye Sync Tick: DeadEye=" + std::to_string(targetDeadEye));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), static_cast<int>(targetDeadEye));      
    }
}

bool IsHorseBeingLed(Ped playerPed, Ped horsePed) {
	// Assumes both playerPed and horsePed are valid and exist
	bool isBeingLed = TASK::_IS_PED_BEING_LED(horsePed);
	bool isPlayerLeading = TASK::_IS_PED_LEADING_HORSE(playerPed);

	Ped ledHorse = TASK::_GET_LED_HORSE_FROM_PED(playerPed);

	WriteLog(LogLevel::Dev, "Horse Being Led Check: PlayerPed=" + std::to_string(playerPed) + ", HorsePed=" + std::to_string(horsePed) + ", IsBeingLed=" + std::to_string(isBeingLed) + ", IsPlayerLeading=" + std::to_string(isPlayerLeading) + ", ledHorse=" + std::to_string(ledHorse));

    return isBeingLed && isPlayerLeading && ledHorse == horsePed;
}

bool CalculateHorseLeadReward(Ped horsePed, bool isBeingLed, bool isHorseHealthGold, float hoursDelta, float currentHealth) {
    // Edge Trigger: The player is actively leading the horse on this specific loop pass accumulating a reward cache for later application if the core is normal white
    if (isBeingLed) {
        float tickReward = HORSE_LEAD_REWARD_PER_GAME_HOUR * hoursDelta;
        g_State.accumulatedHorseLeadHealthReward += tickReward;

        WriteLog(LogLevel::Verbose, "Player is leading horse. Accruing reward cache unconditionally: +" + std::to_string(tickReward) + " (Total Cached: " + std::to_string(g_State.accumulatedHorseLeadHealthReward) + ")");
        g_State.wasHorseLeadingLastTick = true;
    }
    // Edge Trigger: The player JUST stopped leading the horse on this specific loop pass applying the cached reward if the core is normal white, or discarding it if the core is golden
    else if (!isBeingLed && g_State.wasHorseLeadingLastTick) {
        if (g_State.accumulatedHorseLeadHealthReward > 0.0f) {
            if (!isHorseHealthGold) {
                // Apply the full accumulated reward legacy now that the core is normal white
                int finalHealth = static_cast<int>(std::min(currentHealth + g_State.accumulatedHorseLeadHealthReward, 100.0f));

                WriteLog(LogLevel::Standard, "Player stopped leading horse. Core is normal white; applying cached reward: +" + std::to_string(g_State.accumulatedHorseLeadHealthReward) + ". Horse Health updated to: " + std::to_string(finalHealth));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), finalHealth);
            }
            else {
                WriteLog(LogLevel::Verbose, "Player stopped leading horse. Core is currently Golden; cached reward of " + std::to_string(g_State.accumulatedHorseLeadHealthReward) + " points discarded safely.");
            }
        }

        // Clean out cache tracking parameters completely
        g_State.accumulatedHorseLeadHealthReward = 0.0f;
        g_State.wasHorseLeadingLastTick = false;

		return !isHorseHealthGold; // Return true if the reward was applied (core was normal white), false if it was discarded (core was golden)
    }

    return false;
}

float CalculateHorseSpeedStaminaDecayMultiplier(Ped horsePed, const HorseConfigSettings& horseConfig) {
    float currentSpeed = ENTITY::GET_ENTITY_SPEED(horsePed);
    float speedMultiplier = 1.0f;

    // The logic automatically scales down to a base flat rate if the horse is hitched or stabled!
    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        speedMultiplier = horseConfig.gallopStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Running/Galloping. Applying high stamina multiplier.");
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot) && currentSpeed <= static_cast<float>(HorseSpeed::Gallop)) {
        speedMultiplier = horseConfig.trotStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Trotting. Applying medium stamina multiplier.");
    }
    else {
        WriteLog(LogLevel::Verbose, "Active Horse: Hitched, standing still, or grazing. Applying base decay.");
    }

	return speedMultiplier;
}

float CalculateHorseStaminaDecayMultiplier(Ped horsePed, const HorseConfigSettings& horseConfig, bool isPlayerSemiRestrained, const PlayerConfigSettings& playerConfig) {
	float multiplier = CalculateHorseSpeedStaminaDecayMultiplier(horsePed, horseConfig);
    
    // Dynamic Velocity Evaluation (Works whether you are riding it or it's moving ambiently)
    if (isPlayerSemiRestrained)
        multiplier += playerConfig.campedMultiplier;

    return multiplier;
}

void ProcessHorseSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {
    auto [horsePed, hasHorse] = GetActivePlayerHorse(playerPed);

	if (!hasHorse)
    {
		WriteLog(LogLevel::Verbose, "No active horse detected for the player. Skipping horse core simulation.");
        return;
    }

    bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Health));
    bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Stamina));

    float currentHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health)));
    float currentStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina)));

    WriteLog(LogLevel::Verbose, "Horse Core Simulation: isHealthGold = " + std::to_string(isHorseHealthGold) + ", isStaminaGold = " + std::to_string(isHorseStaminaGold));

    bool isBeingLed = IsHorseBeingLed(playerPed, horsePed);
	bool recievedHorseLeadReward = CalculateHorseLeadReward(horsePed, isBeingLed, isHorseHealthGold, hoursDelta, currentHealth);    
    
    float liveHorseHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health)));

	// Health Core Decay Logic (only applies if the horse's health core is not golden, it's not being led, and has no cached lead reward was just applied)
    if (!isHorseHealthGold && !isBeingLed && !recievedHorseLeadReward) {
        float activeHealthDecay = horseConfig.baseHealthDecay;

		if (ctx.isAtCamp)
			activeHealthDecay *= playerConfig.campedMultiplier;

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
		float minHealthFloor = ctx.isAtCamp ? horseConfig.campedHealthFloor : 0.0f; //the player is semi restrained (jail/camp/bath), so we apply the horse's restrained floor to prevent full depletion

        targetHealth = std::max(targetHealth, minHealthFloor);

        WriteLog(LogLevel::Verbose, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), (int)targetHealth);
    }

	// Stamina Core Decay Logic (only applies if the horse's stamina core is not golden, and has no cached lead reward was just applied)
    if (!isHorseStaminaGold && !recievedHorseLeadReward) {
        float targetStamina = currentStamina;

        // If the horse is physically hitched, freeze its stamina completely!
		// Apply a small bonus if the horse is well-fed or health is golden to reward the player for good care
        // This rewards the player for using hitching posts around the world.
        
        if (g_State.wasHorseHitchedLastTick) {
            WriteLog(LogLevel::Verbose, "Active Horse is securely hitched. Freezing stamina core decay completely.");

            // Give a little bonus to stamina is horse is well fed
            if (isHorseHealthGold || liveHorseHealth >= HORSE_HITCHED_HEALTH_REQ) {
				float rewardMultiplier = (HORSE_HITCHED_BONUS_MULT * hoursDelta);
                targetStamina = currentStamina + rewardMultiplier;

				WriteLog(LogLevel::Verbose, "Horse is well-fed or has golden health. Applying hitched bonus: +" + std::to_string(rewardMultiplier) + " to stamina core.");                
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
            }
        }
        else {
            // Normal dynamic movement decay route
            float activeStaminaDecay = horseConfig.baseStaminaDecay * CalculateHorseStaminaDecayMultiplier(horsePed, horseConfig, ctx.isAtCamp, playerConfig);
            
            float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
            float minStaminaFloor = ctx.isAtCamp ? horseConfig.campedStaminaFloor : 0.0f;

            targetStamina = std::max(targetStamina, minStaminaFloor);

            WriteLog(LogLevel::Verbose, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay=" + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor));
            WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Stamina=" + std::to_string(targetStamina));
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
        }
    }    
}

void ProcessOpenWorldTimeSkip(Ped playerPed, int currentTotalMinutes, const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {
    int elapsedMinutes = currentTotalMinutes - g_State.cachedInGameTimeMinutes;
    if (elapsedMinutes < 0) elapsedMinutes += 1440;

    if (elapsedMinutes > 0) {
		WriteLog(LogLevel::Verbose, "Open-world time skip detected. Elapsed game minutes: " + std::to_string(elapsedMinutes));

        // Reads LIVE data from the engine at the moment of travel
        float liveHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
        float liveStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
        float liveDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));

		WriteLog(LogLevel::Verbose, "Player Live Core Values: Health=" + std::to_string(liveHealth) + ", Stamina=" + std::to_string(liveStamina) + ", DeadEye=" + std::to_string(liveDeadEye));

        // We check horse baseline stats live too
        auto [activeHorse, hasHorse] = GetActivePlayerHorse(playerPed);

        float liveHorseHealth = hasHorse ? static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health))) : 0.0f;
        float liveHorseStamina = hasHorse ? static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina))) : 0.0f;

		WriteLog(LogLevel::Verbose, "Horse Live Core Values: Health=" + std::to_string(liveHorseHealth) + ", Stamina=" + std::to_string(liveHorseStamina) + ", HasHorse=" + std::to_string(hasHorse));

        // Route directly into the shared engine (Never sleeping during open-world transits)
        ApplyBatchTimeSkipDecay(playerPed, elapsedMinutes,
            liveHealth, liveStamina, liveDeadEye,
            liveHorseHealth, liveHorseStamina, hasHorse,
            generalConfig, playerConfig, horseConfig);
    }

    // This updates the transition baseline AND keeps CalculateTimescaleDelta from dual-decaying
    g_State.cachedInGameTimeMinutes = currentTotalMinutes;
    g_State.lastTickInGameMinutes = currentTotalMinutes;
}

void ResetSimulationState() {
    g_State.isRestricted = false;
    g_State.isRestrictedVariable = false;

    // Clear out the timescale tracking explicitly in one place
    g_State.cachedInGameTimeMinutes = GetTotalInGameMinutes();
    g_State.lastTickInGameMinutes = 0;
    g_State.timescaleDeltaInitialised = false;

    g_State.accumulatedTimeMs = 0;
    g_State.cachedHealth = 100.0f;
    g_State.cachedStamina = 100.0f;
    g_State.cachedDeadEye = 100.0f;    
    g_State.cachedMoney = 0;

	g_State.cachedBathingState = 0;

    g_State.cachedHorseHealth = 100.0f;
    g_State.cachedHorseStamina = 100.0f;

    g_State.hadHorse = false;

    g_State.accumulatedHorseLeadHealthReward = 0.0f;
    g_State.wasHorseLeadingLastTick = false;
    g_State.accumulatedHorseHitchedImmobility = 0;
	g_State.wasHorseHitchedLastTick = false;

    g_State.wasSleepingDuringRestriction = false;
    g_State.wasAtCampDuringRestriction = false;
	g_State.wasInMissionDuringRestriction = false;
	g_State.wasInMinigameDuringRestriction = false;
    g_State.isBathingDuringRestriction = false;

	g_State.PostMissionDecayHoursRemaining = 0.0f;

    g_State.wasRestrictedLastTick = false;

    g_State.lastKnownPlayerPed = 0;

    for (auto& [type, monitor] : g_activeMonitors)
        monitor.Reset();

    s_structuresInitialized = false;    

    ClearDevLogCache();
}


void CheckForConfigChange() {
    if (g_ShouldReloadConfig.load(std::memory_order_acquire) || !s_structuresInitialized) {
        // Log an accurate message based on WHY we are filling the cache
        if (!s_structuresInitialized)
            WriteLog(LogLevel::Verbose, "Initializing local simulation configuration cache for the first time...");
        else
            WriteLog(LogLevel::Verbose, "Dynamic hot-reload requested. Updating configuration cache...");

        std::lock_guard<std::mutex> lock(g_ConfigMutex);

        // Cache the global configuration structures for fast-path access in the main loop
        // they should not be used anywhere else except for the main loop tick, as they are not thread-safe
        s_cachedCoreConfig = g_CoreConfig;
        s_cachedGeneralConfig = g_GeneralConfig;
        s_cachedPlayerConfig = g_PlayerConfig;
        s_cachedHorseConfig = g_HorseConfig;

        s_structuresInitialized = true;
        g_ShouldReloadConfig.store(false, std::memory_order_release);
    }
}

bool IsInSaveLoadState(Ped playerPed) {
    Player playerId = PLAYER::PLAYER_ID();

    // DETECTION PHASE
    bool isFreshSaveLoad = (g_State.lastKnownPlayerPed != 0 && g_State.lastKnownPlayerPed != playerPed);
    bool isDead = PLAYER::IS_PLAYER_DEAD(playerId) || !ENTITY::DOES_ENTITY_EXIST(playerPed);
    bool isLoading = DLC::GET_IS_LOADING_SCREEN_ACTIVE();

	WriteLog(LogLevel::Dev, "Save/Load State Check: isFreshSaveLoad=" + std::to_string(isFreshSaveLoad) + ", isDead=" + std::to_string(isDead) + ", isLoading=" + std::to_string(isLoading));

    // HARD RESET ENGINE
    if (isLoading || isDead || isFreshSaveLoad) {
        if (g_State.timescaleDeltaInitialised) {
            WriteLog(LogLevel::Standard, "Hard Reset Event Verified (Save Load/Death). Purging cache cleanly...");
            ResetSimulationState();
        }

        g_State.lastKnownPlayerPed = playerPed;

        return true;
    } 
    // RE-ANCHOR ENGINE
    else if(!g_State.timescaleDeltaInitialised) {
        WriteLog(LogLevel::Standard, "Simulation baseline calibrated. Re-anchoring timestamps...");

		g_State.cachedInGameTimeMinutes = GetTotalInGameMinutes();
        g_State.lastTickInGameMinutes = g_State.cachedInGameTimeMinutes;
        g_State.accumulatedTimeMs = 0;

        g_State.timescaleDeltaInitialised = true;
        g_State.lastKnownPlayerPed = playerPed;

        return true;
    }

    g_State.lastKnownPlayerPed = playerPed;

	return false;
}

void UpdateCoreSimulation() {
    Ped playerPed = PLAYER::PLAYER_PED_ID();

	if (IsInSaveLoadState(playerPed))
		return;

    CheckForConfigChange();

    // Cache the global configuration structures for fast-path access in the main loop
    CoreConfigSettings    coreConfig = s_cachedCoreConfig;
    GeneralConfigSettings generalConfig = s_cachedGeneralConfig;
    PlayerConfigSettings  playerConfig = s_cachedPlayerConfig;
    HorseConfigSettings   horseConfig = s_cachedHorseConfig;

    GameplayContext context = EvaluateGameplayState(playerPed, generalConfig);
    int currentTotalMinutes = GetTotalInGameMinutes();

    // FAST-PATH BOUNDARY TRANSITIONS (Forcing immediate responses to missions or travel updates)
    if (context.freezeActive != g_State.wasRestrictedLastTick) {
        WriteLog(LogLevel::Verbose, "Fundamental state boundary flipped! Forcing immediate cache transition process.");
        
        HandleStateTransitions(playerPed, context, currentTotalMinutes, generalConfig, playerConfig, horseConfig);

        g_State.wasRestrictedLastTick = context.freezeActive;
        g_State.accumulatedTimeMs = 0;
    }
    // If we're still in a restricted state and the freeze variable is active, we update the frozen state variables to ensure they reflect the current gameplay context.
    else if (g_State.isRestricted && g_State.isRestrictedVariable) {
        UpdateFrozenStateVariables();
    }
    // SLOW-PATH APP DECAY SIMULATION (Accumulates interval smoothly over normal open world play)
    else if (!context.freezeActive) {
        g_State.accumulatedTimeMs += coreConfig.stateChangeTickIntervalMs;

        if (g_State.accumulatedTimeMs >= coreConfig.coreDrainTickIntervalMs) {
            float hoursDelta = CalculateTimescaleDelta(currentTotalMinutes);

            if (hoursDelta == 0.0f) {
                // The real-world interval passed, but the in-game integer clock hasn't ticked up yet.
                // Do NOT reset accumulatedTimeMs. Let it retry on the next 500ms loop pass.
                WriteLog(LogLevel::Verbose, "Interval boundary hit, but game engine clock hasn't rolled to a new integer minute. Holding accumulation cache.");
            }
            // If time actually moved forward in the engine space, process simulation and reset
            else if (hoursDelta > 0.0f && hoursDelta < 24.0f) {
				WriteLog(LogLevel::Verbose, "Processing normal core simulation for elapsed hours: " + std::to_string(hoursDelta));

                ProcessPlayerSimulation(playerPed, hoursDelta, context, generalConfig, playerConfig);
                ProcessHorseSimulation(playerPed, hoursDelta, context, playerConfig, horseConfig);

                // Only clear accumulation on successful simulation cycles!
                g_State.accumulatedTimeMs = 0;
            } 
            // If the delta is negative, it indicates a macro time jump (e.g., train, stagecoach, or fast travel)
			else {
                WriteLog(LogLevel::Standard, "Macro time jump detected during active play (Train/Stagecoach). Triggering batch catch-up.");

                // We force a manual time skip evaluation using the global cache states
                // by temporarily simulating a transition block or a direct clean helper.
                ProcessOpenWorldTimeSkip(playerPed, currentTotalMinutes, generalConfig, playerConfig, horseConfig);

                g_State.accumulatedTimeMs = 0; // Clear the accumulation safely
            }
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
    g_ThreadManager.Initialize();    

    WriteLog(LogLevel::Standard, "==== Core Drain Management Simulation Engine Initialised Successfully [" + VERSION + "] ====");	
        
    while (true) {
        UpdateCoreSimulation();
        scriptWait(g_stateChangeTickIntervalMs.load(std::memory_order_acquire));
    }
}