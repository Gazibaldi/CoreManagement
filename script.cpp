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
#include <span>
#include <unordered_map>
#include <cstdlib>
#include <array>
#include "..\CoreSDK\inc\natives.h"
#include "script.h"
#include "Logger.h"
#include "Config.h"
#include "ThreadManager.h"
#include "CMTypes.h"
#include "Constants.h"
#include "TransientActivityMonitor.h"
#include "MovementVelocityTracker.h"
#include "HorseManager.h"

enum class CoreIndex : int { Health = 0, Stamina = 1, DeadEye = 2 };
enum class HorseSpeed : int { Trot = 2, Gallop = 5 };
enum class PlayerSpeed : int { Jog = 3, Sprint = 5 };
enum class HorseTackSlot : int { Saddle = 2, Stirrups = 6 };

SimulationState g_State;
MovementVelocityTracker g_MovementTracker;

static CoreConfigSettings    s_cachedCoreConfig;
static GeneralConfigSettings s_cachedGeneralConfig;
static PlayerConfigSettings  s_cachedPlayerConfig;
static HorseConfigSettings   s_cachedHorseConfig;
static bool s_structuresInitialized = false;

std::array<Hash, 17> g_SleepScenarios{};
std::array<Hash, 16> g_CampScenarios{};
std::unordered_map<TransientActivityType, TransientActivityMonitor> g_activeMonitors{};

std::array<PlayerPassiveModifier, 4> g_PlayerPassiveModifiers{};

std::array<TackModifier, 12> g_StirrupModifiers{};
std::array<TackModifier, 24> g_SaddleModifiers{};

Hash g_PokerScriptHash;
Hash g_BlackjackScriptHash;
Hash g_DominoesScriptHash;
Hash g_FiveFingerFilletScriptHash;

Hash g_NullSlotHash;

Hash g_CharacterHash;
Hash g_wardrobeSlotHash;

Hash g_ActiveHorseSlotHash;

Hash GetKey(const char* key) {
	Hash hash = MISC::GET_HASH_KEY(key);
	WriteLog(LogLevel::Dev, "Precomputed hash for key '" + std::string(key) + "': " + std::to_string(hash));
    
    return hash;
}

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

    g_PlayerPassiveModifiers = { {
            { "UPGRADE_TRINKET_OWL_FEATHER",          GetKey("UPGRADE_TRINKET_OWL_FEATHER"), 0.15f, 0.15f, 0.15f },
            { "PROVISION_TALISMAN_BEAR_CLAW",         GetKey("PROVISION_TALISMAN_BEAR_CLAW"), 0.10f, 0.00f, 0.00f },
            { "PROVISION_TALISMAN_BISON_HORN",        GetKey("PROVISION_TALISMAN_BISON_HORN"), 0.00f, 0.10f, 0.00f },
            { "PROVISION_TALISMAN_ALLIGATOR_TOOTH",   GetKey("PROVISION_TALISMAN_ALLIGATOR_TOOTH"), 0.00f, 0.00f, 0.10f }
        } };

    g_StirrupModifiers = { {
        { "UPGRADE_HORSE_STIRRUPS_FILLIES",         GetKey("UPGRADE_HORSE_STIRRUPS_FILLIES"), 0.00f, 0.00f },
        { "UPGRADE_HORSE_STIRRUPS_BAROQUE",         GetKey("UPGRADE_HORSE_STIRRUPS_BAROQUE"), 0.00f, 0.05f },
        { "UPGRADE_HORSE_STIRRUPS_SLIM_LINE_IRON",  GetKey("UPGRADE_HORSE_STIRRUPS_SLIM_LINE_IRON"), 0.00f, 0.10f },
        { "UPGRADE_HORSE_STIRRUPS_SLIM_LINE",       GetKey("UPGRADE_HORSE_STIRRUPS_SLIM_LINE"), 0.00f, 0.15f },
        { "UPGRADE_HORSE_STIRRUPS_DEEP_FLANGE",     GetKey("UPGRADE_HORSE_STIRRUPS_DEEP_FLANGE"), 0.00f, 0.20f },
        { "UPGRADE_HORSE_STIRRUPS_IRON_ROUND",      GetKey("UPGRADE_HORSE_STIRRUPS_IRON_ROUND"), 0.00f, 0.25f },
        { "UPGRADE_HORSE_STIRRUPS_OXBOW",           GetKey("UPGRADE_HORSE_STIRRUPS_OXBOW"), 0.00f, 0.30f },
        { "UPGRADE_HORSE_STIRRUPS_OXBOW_TAPADERA",  GetKey("UPGRADE_HORSE_STIRRUPS_OXBOW_TAPADERA"), 0.00f, 0.35f },
        { "UPGRADE_HORSE_STIRRUPS_BELL",            GetKey("UPGRADE_HORSE_STIRRUPS_BELL"), 0.00f, 0.40f },
        { "UPGRADE_HORSE_STIRRUPS_SAFETY",          GetKey("UPGRADE_HORSE_STIRRUPS_SAFETY"), 0.00f, 0.40f },
        { "UPGRADE_HORSE_STIRRUPS_BELL_TAPADERA",   GetKey("UPGRADE_HORSE_STIRRUPS_BELL_TAPADERA"), 0.00f, 0.45f },
        { "UPGRADE_HORSE_STIRRUPS_HOODED",          GetKey("UPGRADE_HORSE_STIRRUPS_HOODED"), 0.00f, 0.50f }
    } };

    g_SaddleModifiers = { {
            // ==========================================
            // STANDARD / UPGRADED STABLE SADDLES
            // ==========================================
            { "SADDLE_KNELLER_DAKOTA_STOCK",            GetKey("SADDLE_KNELLER_DAKOTA_STOCK"), 0.04f, 0.08f },
            { "SADDLE_KNELLER_DAKOTA_IMPROVED",         GetKey("SADDLE_KNELLER_DAKOTA_IMPROVED"), 0.12f, 0.16f },

            { "SADDLE_KNELLER_MOTHER_HUBBARD_STOCK",    GetKey("SADDLE_KNELLER_MOTHER_HUBBARD_STOCK"), 0.06f, 0.04f },
            { "SADDLE_KNELLER_MOTHER_HUBBARD_IMPROVED", GetKey("SADDLE_KNELLER_MOTHER_HUBBARD_IMPROVED"), 0.14f, 0.12f },

            { "SADDLE_LUMLEY_MCLELLAND_STOCK",          GetKey("SADDLE_LUMLEY_MCLELLAND_STOCK"), 0.04f, 0.06f },
            { "SADDLE_LUMLEY_MCLELLAND_IMPROVED",       GetKey("SADDLE_LUMLEY_MCLELLAND_IMPROVED"), 0.14f, 0.18f },

            { "SADDLE_LUMLEY_RANCH_CUTTER_STOCK",       GetKey("SADDLE_LUMLEY_RANCH_CUTTER_STOCK"), 0.08f, 0.04f },
            { "SADDLE_LUMLEY_RANCH_CUTTER_IMPROVED",    GetKey("SADDLE_LUMLEY_RANCH_CUTTER_IMPROVED"), 0.14f, 0.12f },

            { "SADDLE_STENGER_ROPER_STOCK",             GetKey("SADDLE_STENGER_ROPER_STOCK"), 0.10f, 0.04f },
            { "SADDLE_STENGER_ROPER_IMPROVED",          GetKey("SADDLE_STENGER_ROPER_IMPROVED"), 0.18f, 0.12f },

            { "SADDLE_GERDEN_TRAIL_STOCK",              GetKey("SADDLE_GERDEN_TRAIL_STOCK"), 0.04f, 0.10f },
            { "SADDLE_GERDEN_TRAIL_IMPROVED",           GetKey("SADDLE_GERDEN_TRAIL_IMPROVED"), 0.12f, 0.18f },

            { "SADDLE_GERDEN_VAQUERO_STOCK",            GetKey("SADDLE_GERDEN_VAQUERO_STOCK"), 0.06f, 0.08f },
            { "SADDLE_GERDEN_VAQUERO_IMPROVED",         GetKey("SADDLE_GERDEN_VAQUERO_IMPROVED"), 0.14f, 0.16f },

            // ==========================================
            // TRAPPER CRAFTED SADDLES
            // ==========================================
            { "SADDLE_BEAVER_ROPING",                   GetKey("SADDLE_BEAVER_ROPING"), 0.26f, 0.20f },
            { "SADDLE_ALLIGATOR_CUTTER",                GetKey("SADDLE_ALLIGATOR_CUTTER"), 0.24f, 0.20f },
            { "SADDLE_BOAR_MOTHER_HUBBARD",             GetKey("SADDLE_BOAR_MOTHER_HUBBARD"), 0.22f, 0.20f },
            { "SADDLE_COUGAR_MCLELLAND",                GetKey("SADDLE_COUGAR_MCLELLAND"), 0.20f, 0.22f },
            { "SADDLE_PANTHER_TRAIL",                   GetKey("SADDLE_PANTHER_TRAIL"), 0.20f, 0.26f },
            { "SADDLE_RATTLESNAKE_VAQUERO",             GetKey("SADDLE_RATTLESNAKE_VAQUERO"), 0.22f, 0.24f },
            { "SADDLE_BEAR_DAKOTA",                     GetKey("SADDLE_BEAR_DAKOTA"), 0.20f, 0.24f },

            // ==========================================
            // SPECIAL EDITION / ULTIMATE EDITION SADDLES
            // ==========================================
            { "SADDLE_NUEVO_PARAISO",                   GetKey("SADDLE_NUEVO_PARAISO"), 0.12f, 0.12f },
            { "SADDLE_HIGH_PLAINS_CUTTING",             GetKey("SADDLE_HIGH_PLAINS_CUTTING"), 0.12f, 0.12f },

            // ==========================================
            // PASSIVE INVENTORY ITEMS / TALISMANS
            // ==========================================
            { "PROVISION_TALISMAN_BOAR_TUSK",           GetKey("PROVISION_TALISMAN_BOAR_TUSK"), 0.10f, 0.10f, true }
        } };

    g_NullSlotHash = GetKey("SLOTID_NONE");

    g_CharacterHash = GetKey("CHARACTER");
    g_ActiveHorseSlotHash = GetKey("SLOTID_ACTIVE_HORSE");

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

bool IsInActiveScenario(Ped playerPed, std::span<const Hash> scenarioSet) {
    Hash activeScenario = TASK::_GET_SCENARIO_POINT_TYPE_PED_IS_USING(playerPed);
	WriteLog(LogLevel::Dev, "Active scenario hash: " + std::to_string(activeScenario));

    // Fall back to transient dynamic actions if the primary structural anchor returns empty
	if (activeScenario == 0) {
		activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);
		WriteLog(LogLevel::Dev, "Fallback to dynamic scenario hash: " + std::to_string(activeScenario));
	}

	if (activeScenario == 0) return false;

    auto contains_check = [activeScenario](Hash scenarioHash) {
        return scenarioHash == activeScenario;
    };

    bool isInScenarioSet = std::ranges::any_of(scenarioSet, contains_check);

	// If the current log level is below Dev, we skip the detailed logging and just return the result.
    if (g_CoreConfig.currentLogLevel < LogLevel::Dev)
        return isInScenarioSet;

    if (isInScenarioSet) {
		WriteLog(LogLevel::Dev, "Active scenario matches known set");
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

            WriteLog(LogLevel::Verbose, "Player jailed check: Player has exited jail. Resetting transient jailed state.");
            return false;
        }

        WriteLog(LogLevel::Verbose, "Player jailed check: Player is still in jail");
        return true;
    }
    else {
        if(jailMonitor.ActivateMonitor()) {
            WriteLog(LogLevel::Verbose, "Player is being arrested and is in custody.");
            return true;
        }

        WriteLog(LogLevel::Verbose, "Player is not being arrested or is not in custody.");
        return false;
    }
}

RDR_GUID GetPlayerInventoryGUID() {
    RDR_GUID emptyGuid{}; // Create a blank container
    RDR_GUID playerGuid{};

    // 1 = Player Inventory ID
    if(INVENTORY::INVENTORY_GET_GUID_FROM_ITEMID(1, (Any*)&emptyGuid, g_CharacterHash, g_NullSlotHash, (Any*)&playerGuid))
        return playerGuid;

    return RDR_GUID{};
}

RDR_GUID GetHorseInventoryGUID() {
    /*RDR_GUID playerGuid = GetPlayerInventoryGUID();
    RDR_GUID horseGuid{};

    if (playerGuid.IsInvalid()) {
        WriteLog(LogLevel::Dev, "GetHorseInventoryGUID failed: Parent player inventory GUID is invalid.");
        return RDR_GUID{};
    }*/

    RDR_GUID emptyGuid{}; // Create a blank container
    RDR_GUID horseGuid{};

    // 1 = Player Inventory ID
    if (INVENTORY::INVENTORY_GET_GUID_FROM_ITEMID(1, (Any*)&emptyGuid, g_ActiveHorseSlotHash, g_NullSlotHash, (Any*)&horseGuid))
        return horseGuid;

    return RDR_GUID{};
}

bool DoesInventoryContainItem(int invID, Hash itemHash, const std::string& itemName) {
    RDR_ITEMINFO itemInfo{};

    WriteLog(LogLevel::Dev, "Checking for item: " + itemName + " (hash: " + std::to_string(itemHash) + ")");

    int count = INVENTORY::_INVENTORY_GET_INVENTORY_ITEM_COUNT_WITH_ITEMID(invID, itemHash, false);
	
    if (count > 0) {
		WriteLog(LogLevel::Dev, "Item " + itemName + " found in inventory. Count: " + std::to_string(count));
		return true;
	}
    else
    {
		WriteLog(LogLevel::Dev, "Item " + itemName + " not found in inventory.");
		return false;
    }
}

bool DoesPlayerHaveItem(Hash itemHash, const std::string& itemName) {
   return DoesInventoryContainItem(1, itemHash, itemName);
}

bool DoesHorseHaveItem(Ped horsePed, HorseTackSlot slot, Hash itemHash, const std::string& itemName) {
    int invID = INVENTORY::_INVENTORY_GET_INVENTORY_ID_FROM_PED(horsePed);

	WriteLog(LogLevel::Dev, "Horse inventory ID: " + std::to_string(invID));

	return DoesInventoryContainItem(invID, itemHash, itemName);
}

CombinedPlayerCoreModifiers GetActivePlayerPassiveModifiers() {
    CombinedPlayerCoreModifiers modifiers{};

    for (const auto& passive : g_PlayerPassiveModifiers) {
        // All talismans/trinkets live in the wardrobe data tree branch
        if (DoesPlayerHaveItem(passive.itemHash, std::string(passive.name))) {
            modifiers.totalHealthReduction += passive.healthCoreMod;
            modifiers.totalStaminaReduction += passive.staminaCoreMod;
            modifiers.totalDeadEyeReduction += passive.deadEyeCoreMod;
        }
    }

    return modifiers;
}

CombinedTackModifiers GetActiveHorseTackModifiers(Ped horsePed) {
    CombinedTackModifiers modifiers{};

     // Scan for Stirrups
    for (const auto& stirrup : g_StirrupModifiers) {
        if (DoesHorseHaveItem(horsePed, HorseTackSlot::Stirrups, stirrup.itemHash, std::string(stirrup.name))) {
            modifiers.totalStaminaReduction += stirrup.staminaCoreMod;
            break;
        }
    }

    // Scan for Saddles & Horse Player Passives
    for (const auto& saddle : g_SaddleModifiers) {
        bool hasItem = false;

        if (saddle.isPlayerPassive) {
            hasItem = DoesPlayerHaveItem(saddle.itemHash, std::string(saddle.name));
        }
        else {
            hasItem = DoesHorseHaveItem(horsePed, HorseTackSlot::Saddle, saddle.itemHash, std::string(saddle.name));
        }

        if (hasItem) {
            modifiers.totalHealthReduction += saddle.healthCoreMod;
            modifiers.totalStaminaReduction += saddle.staminaCoreMod;
        }
    }

    return modifiers;
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

            WriteLog(LogLevel::Verbose, "Player bathing check: Player has exited the bath. Resetting transient bathing state.");
            return false;
        }

        WriteLog(LogLevel::Verbose, "Player bathing check: Player is still in a bath");
        return true;
    }
    else {
        if (bathingMonitor.ActivateMonitor()) {
            WriteLog(LogLevel::Verbose, "Player bathing check: Player is now in a bath.");
            return true;
        }

        WriteLog(LogLevel::Verbose, "Player bathing check: Player is not in a bath.");
        return false;
    }
}

void RecordTrackingSnapshots(Ped playerPed) {
	g_MovementTracker.RecordSnapshot(playerPed);
	g_HorseManager.RecordSnapshot(playerPed);
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

    if (!ctx.freezeActive)
		RecordTrackingSnapshots(playerPed);
    
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

bool IsPlayerMounted(Ped playerPed) {
    auto [horsePed, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed, ActiveHorseType::Mounted);
    
    return hasHorse;
}

float GetPlayerMountedStaminaMultiplier(const PlayerConfigSettings& playerConfig) {
    float currentSpeed = g_MovementTracker.GetHorseAverageSpeed();

    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        WriteLog(LogLevel::Verbose, "Active horse is galloping. Applying high Player stamina decay multiplier");
        return playerConfig.mountGallopStaminaMultiplier;
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot)) {
        WriteLog(LogLevel::Verbose, "Active horse is trotting. Applying base Player stamina decay multiplier");
        return playerConfig.mountTrotStaminaMultiplier;
    }
    else {
        WriteLog(LogLevel::Verbose, "Active horse is walking or stationary. Applying low Player stamina decay multiplier");
        return playerConfig.mountWalkStaminaMultiplier;
    }
}

float GetPlayerRunningStaminaMultiplier(const PlayerConfigSettings& playerConfig) {
	float currentSpeed = g_MovementTracker.GetPlayerOnFootSpeed();

    if (currentSpeed > static_cast<float>(PlayerSpeed::Sprint)) {
        WriteLog(LogLevel::Verbose, "Player is sprinting. Applying high Player stamina decay multiplier");
        return playerConfig.sprintStaminaMultiplier;
    }
    else if (currentSpeed > static_cast<float>(PlayerSpeed::Jog)) {
        WriteLog(LogLevel::Verbose, "Player is jogging. Applying medium Player stamina decay multiplier");
        return playerConfig.jogStaminaMultiplier;
    }
    else {
        WriteLog(LogLevel::Verbose, "Player is walking or stationary. Applying base Player stamina decay multiplier");
        return 1.0f;
    }
}

float CalculateHorseSpeedStaminaDecayMultiplier(const HorseConfigSettings& horseConfig) {
    float currentSpeed = g_MovementTracker.GetHorseAverageSpeed();
    float speedMultiplier = 1.0f;

    // The logic automatically scales down to a base flat rate if the horse is hitched or stabled!
    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        speedMultiplier = horseConfig.gallopStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Running/Galloping. Applying high stamina multiplier.");
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot)) {
        speedMultiplier = horseConfig.trotStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Trotting. Applying medium stamina multiplier.");
    }
    else {
		speedMultiplier = horseConfig.grazeStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Walking/Grazing. Applying base decay.");
    }

    return speedMultiplier;
}

float CalculatePlayerHealthDecayMultiplier(const PlayerConfigSettings& playerConfig, bool isPlayerSemiRestrained = false, bool applyPostMissionDecay = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained) {
        WriteLog(LogLevel::Verbose, "Player is semi-restrained. Applying restrained health decay multiplier.");
        multiplier = playerConfig.campedMultiplier;
    }

    if (applyPostMissionDecay) {
        WriteLog(LogLevel::Verbose, "Player is exiting a mission. Applying post-mission health decay multiplier.");
        multiplier += POST_MISSION_DECAY_MULTIPLIER;
    }
    
    CombinedPlayerCoreModifiers passives = GetActivePlayerPassiveModifiers();
	WriteLog(LogLevel::Verbose, "Active player passive modifiers: Health Reduction = " + std::to_string(passives.totalHealthReduction));
    multiplier -= passives.totalHealthReduction;

	return std::max(multiplier, 0.1f);
}

float CalculatePlayerStaminaDecayMultiplier(const PlayerConfigSettings& playerConfig, Ped playerPed, bool isPlayerSemiRestrained = false, bool applyPostMissionDecay = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained) {
        WriteLog(LogLevel::Verbose, "Player is semi-restrained. Applying restrained stamina decay multiplier.");
        multiplier = playerConfig.campedMultiplier;
    }
    else if (IsPlayerMounted(playerPed)) {
        float mountedStaminaMultiplier = GetPlayerMountedStaminaMultiplier(playerConfig);
        
        WriteLog(LogLevel::Verbose, "Player is mounted. Applying conditional mounted stamina decay multiplier: " + std::to_string(mountedStaminaMultiplier));
        multiplier = mountedStaminaMultiplier;
    }
    else {
		float runningStaminaMultiplier = GetPlayerRunningStaminaMultiplier(playerConfig);

		WriteLog(LogLevel::Verbose, "Player is on foot. Applying conditional movement stamina decay multiplier: " + std::to_string(runningStaminaMultiplier));
		multiplier = runningStaminaMultiplier;
    }

    if (applyPostMissionDecay) {
        WriteLog(LogLevel::Verbose, "Player is exiting a mission. Applying post-mission stamina decay multiplier.");
        multiplier += POST_MISSION_DECAY_MULTIPLIER;
    }

    CombinedPlayerCoreModifiers passives = GetActivePlayerPassiveModifiers();
    WriteLog(LogLevel::Verbose, "Active player passive modifiers: Stamina Reduction = " + std::to_string(passives.totalStaminaReduction));
    multiplier -= passives.totalStaminaReduction;

    return std::max(multiplier, 0.1f);
}

float CalculatePlayerDeadEyeDecayMultiplier(const PlayerConfigSettings& playerConfig, bool isPlayerSemiRestrained = false, bool applyPostMissionDecay = false, bool isNighttime = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained) {
        WriteLog(LogLevel::Verbose, "Player is semi-restrained. Applying restrained dead eye decay multiplier.");
        multiplier = playerConfig.campedMultiplier;
    }
    else if (isNighttime) {
        WriteLog(LogLevel::Verbose, "Player is in nighttime. Applying night dead eye decay multiplier.");
        multiplier = playerConfig.nightDeadEyeMultiplier;
    }

    if (applyPostMissionDecay) {
        WriteLog(LogLevel::Verbose, "Player is exiting a mission. Applying post-mission dead eye decay multiplier.");
        multiplier += POST_MISSION_DECAY_MULTIPLIER;
    }

    CombinedPlayerCoreModifiers passives = GetActivePlayerPassiveModifiers();
    WriteLog(LogLevel::Verbose, "Active player passive modifiers: Dead Eye Reduction = " + std::to_string(passives.totalDeadEyeReduction));
    multiplier -= passives.totalDeadEyeReduction;

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
        WriteLog(LogLevel::Standard, "Short sleep session detected (< " + std::to_string(MIN_SLEEP_HOURS) + " hours). Restorative stamina uplift capped at 40 points.");
    }

	// DAYTIME WILDERNESS SLEEP PENALTY (Minimum Hours but not in a hotel room)
	// If you sleep for the minimum or more hours but it's daytime and you're not in a hotel room, your maximum reward capacity is capped hard at 50 points.
	if (elapsedHours >= MIN_SLEEP_HOURS && (!isNighttime && !isHotelRoom)) {
        uplift = std::min(uplift, 50.0f);
		WriteLog(LogLevel::Standard, "Daytime wilderness sleep detected. Stamina uplift capped at 50 points.");
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
			WriteLog(LogLevel::Standard, "Luxury hotel room detected at night. Dead Eye caps increased by +20 points.");
            focusCap += 20.0f;
            fatigueCap += 20.0f;
		}
        else {
            WriteLog(LogLevel::Standard, "Luxury hotel room detected during daytime. Dead Eye caps increased by +10 points.");
            focusCap += 10.0f;
            fatigueCap += 10.0f;
        }
    }

	// Daytime Wilderness Penalty (-5 points to caps)
    if (!isNighttime && !isHotelRoom) {
		WriteLog(LogLevel::Standard, "Daytime wilderness sleep detected. Dead Eye caps decreased by -5 points.");
        focusCap -= 5.0f;
        fatigueCap -= 5.0f;
    }

    // THE OVER-SLEEP UNFOCUS ROUTE (OverSleep Threshold Hours)
    if (elapsedHours >= OVERSLEEP_THRESHOLD_HOURS) {
		// If current dead eye is higher than the fatigue boundary, apply the 15-point reduction penalty (5 points if in a hotel room)
        if (currentDeadEye > fatigueCap) {
			WriteLog(LogLevel::Standard, "Over-sleep detected. Dead Eye reduced by 15 points (5 if in a hotel room).");
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

float CalculateHorseHealthDecayMultiplier(Ped horsePed, const PlayerConfigSettings& playerConfig, bool isPlayerSemiRestrained = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained)
        multiplier = playerConfig.campedMultiplier;

    // Registry resolves both the physical saddle and the player's Boar Tusk Talisman automatically
    CombinedTackModifiers tack = GetActiveHorseTackModifiers(horsePed);
    multiplier -= tack.totalHealthReduction;

    return std::max(multiplier, 0.1f);
}

float CalculateHorseStaminaDecayMultiplier(Ped horsePed, const HorseConfigSettings& horseConfig, const PlayerConfigSettings& playerConfig, bool isPlayerSemiRestrained = false) {
    float multiplier = 1.0f;

    if (isPlayerSemiRestrained)
        multiplier = playerConfig.campedMultiplier;
    else 
        multiplier = CalculateHorseSpeedStaminaDecayMultiplier(horseConfig);

    CombinedTackModifiers tack = GetActiveHorseTackModifiers(horsePed);
    multiplier -= tack.totalStaminaReduction;

    return std::max(multiplier, 0.1f);
}

float CalculateHorseStaminaSleepReplenish(float currentStamina, float elapsedHours, bool isNighttime, bool isHotelRoom) {
    // Standard linear scaling: 10 points per hour of sleep
    float uplift = elapsedHours * 10.0f;

    // DAYTIME WILDERNESS SLEEP PENALTY (not in a stable (hotel) room at that)
    // If you sleep for the minimum or more hours but it's daytime and you're not in a hotel room, the horse's maximum reward capacity is capped hard at 50 points.
    if (!isNighttime && !isHotelRoom) {
        uplift = std::min(uplift, 50.0f);
        WriteLog(LogLevel::Standard, "Daytime wilderness sleep detected. Horse Stamina uplift capped at 50 points.");
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
        float activeHealthDecay = playerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(playerConfig);
        float targetHealth = startHealth - (elapsedHours * activeHealthDecay);

        targetHealth = std::max(targetHealth, playerConfig.campedHealthFloor);

        WriteLog(LogLevel::Standard, "Player time-skipped due to bathing. Health decayed to: " + std::to_string(targetHealth));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), static_cast<int>(targetHealth));
    }

    // Freeze standard drain and apply custom uplift reward
    if (!isStaminaGold) {
        float staminaUplift = 15.0f; // Flat 15 point bonus

        if (g_State.cachedBathingState == 2) {
            WriteLog(LogLevel::Standard, "Player Bathing Deluxe Service: Stamina bonus doubled!!");
            staminaUplift = 30.0f; // Double bonus for Deluxe Service (Maid)
        }
        else {
			WriteLog(LogLevel::Standard, "Player Bathing Standard Service: Standard Stamina bonus.");
        }

        float targetStamina = std::min(startStamina + staminaUplift, 100.0f);

        WriteLog(LogLevel::Standard, "Bathing Stamina Uplift Reward Applied: " + std::to_string(targetStamina));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
    }

    // Freeze standard drain and apply custom bath metrics
    if (!isDeadEyeGold) {
        // SPECIAL GOLDEN DEADEYE CORE REWARD (Bathing Deluxe Service (Maid))
        if (g_State.cachedBathingState == 2) {
            WriteLog(LogLevel::Standard, "Player Bathing Deluxe Service: Dead Eye is now golden and fully fortified.");
            ATTRIBUTE::_ENABLE_ATTRIBUTE_CORE_OVERPOWER(playerPed, static_cast<int>(CoreIndex::DeadEye), 100.0f, true);
        }
        else {
			float targetDeadEye = startDeadEye;

            if (startDeadEye < 80.0f) {
                // Calculate the uplift per hour based on the maximum cap of 80.0f over the BATHING_TIMESKIP_MAX_HOURS
				float upliftPerHour = 80.0f / BATHING_TIMESKIP_MAX_HOURS; 

                targetDeadEye = std::min(startDeadEye + (elapsedHours * upliftPerHour), 80.0f);
                WriteLog(LogLevel::Standard, "Bathing Standard Service: Dead Eye capped at a maximum baseline of 80.0f.");
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
        float activeHealthDecay = playerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(playerConfig);
        float targetHealth = startHealth - (elapsedHours * activeHealthDecay);

        float minHealthFloor = wasSleeping ? playerConfig.healthTimeSkipFloor : playerConfig.campedHealthFloor;
        targetHealth = std::max(targetHealth, minHealthFloor);

        WriteLog(LogLevel::Standard, "Player time-skipped. Health decayed to: " + std::to_string(targetHealth));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), static_cast<int>(targetHealth));
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
        float activeStaminaDecay = playerConfig.baseStaminaDecay * CalculatePlayerStaminaDecayMultiplier(playerConfig, playerPed);

        float targetStamina = startStamina - (elapsedHours * activeStaminaDecay);
        targetStamina = std::max(targetStamina, playerConfig.campedStaminaFloor);

        WriteLog(LogLevel::Standard, "Player time-skipped. Stamina decayed to: " + std::to_string(targetStamina));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
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
		// Don't apply nighttime multiplier here, as the player may have moved across the day/night boundary during the time skip, and we want to avoid over-penalizing them for that.
        float activeDeadEyeDecay = playerConfig.baseDeadEyeDecay * CalculatePlayerDeadEyeDecayMultiplier(playerConfig);

        float targetDeadEye = startDeadEye - (elapsedHours * activeDeadEyeDecay);        
        targetDeadEye = std::max(targetDeadEye, playerConfig.campedDeadEyeFloor);

        WriteLog(LogLevel::Standard, "Player time-skipped. Dead Eye decayed to: " + std::to_string(targetDeadEye));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), static_cast<int>(targetDeadEye));
    }

    // Compute Horse Core Reductions

	if (processHorse) {
        auto [activeHorse, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);
        if (hasHorse) {
            bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(activeHorse, static_cast<int>(CoreIndex::Health));
            bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(activeHorse, static_cast<int>(CoreIndex::Stamina));

			// Horse Health Core Decay Logic
            if (!isHorseHealthGold) {
                float activeHealthDecay = horseConfig.baseHealthDecay * CalculateHorseHealthDecayMultiplier(activeHorse, playerConfig);

                float targetHealth = startHorseHealth - (elapsedHours * activeHealthDecay);
                float minHealthFloor = wasSleeping ? horseConfig.healthTimeSkipFloor : horseConfig.campedHealthFloor;

                targetHealth = std::max(targetHealth, minHealthFloor);

                WriteLog(LogLevel::Standard, "Player time-skipped. Horse Health decayed to: " + std::to_string(targetHealth));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health), (int)targetHealth);
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
                // Normal dynamic movement decay route
                float activeStaminaDecay = horseConfig.baseStaminaDecay * CalculateHorseStaminaDecayMultiplier(activeHorse, horseConfig, playerConfig);

                float targetStamina = startHorseStamina - (elapsedHours * activeStaminaDecay);
                
                targetStamina = std::max(targetStamina, horseConfig.campedStaminaFloor);

                WriteLog(LogLevel::Standard, "Player time-skipped. Horse Stamina decayed to: " + std::to_string(targetStamina));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
            }
        }
    }

	// Reset the tracker after applying the batch decay to ensure accurate tracking for the next period
	g_MovementTracker.Reset();
    g_HorseManager.ResetHitchedOrLedTracking();
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
        auto [activeHorse, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);
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
		WriteLog(LogLevel::Verbose, "Post-mission decay is active. Remaining hours: " + std::to_string(g_State.PostMissionDecayHoursRemaining));

		// Ensure that the remaining hours do not go below zero
		g_State.PostMissionDecayHoursRemaining = std::max(g_State.PostMissionDecayHoursRemaining, 0.0f);
	}

    if (!isHealthGold) {
        float activeHealthDecay = playerConfig.baseHealthDecay * CalculatePlayerHealthDecayMultiplier(playerConfig, ctx.isAtCamp, applyPostMissionDecay);

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);

        // The player is semi restrained (jail/camp), so we apply the player's restrained floor to prevent full depletion
		float minHealthFloor = ctx.isAtCamp ? playerConfig.campedHealthFloor : 0.0f;          
        targetHealth = std::max(targetHealth, minHealthFloor);

		WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Player Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), static_cast<int>(targetHealth));
    }

	if (!isStaminaGold) {
        float activeStaminaDecay = playerConfig.baseStaminaDecay * CalculatePlayerStaminaDecayMultiplier(playerConfig, playerPed, ctx.isAtCamp, applyPostMissionDecay);            

        float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
        float minStaminaFloor = ctx.isAtCamp ? playerConfig.campedStaminaFloor : 0.0f;
        targetStamina = std::max(targetStamina, minStaminaFloor);
            
        WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay = " + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor));        
        WriteLog(LogLevel::Standard, "Player Stamina Sync Tick: Stamina=" + std::to_string(targetStamina));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
    }

    if (!isDeadEyeGold) {
        // Normal Open-World, Nighttime, or Semi-Restrained Decay Route
        float activeDeadEyeDecay = playerConfig.baseDeadEyeDecay * CalculatePlayerDeadEyeDecayMultiplier(playerConfig, ctx.isAtCamp, applyPostMissionDecay, isNighttime);

        float targetDeadEye = currentDeadEye - (activeDeadEyeDecay * hoursDelta);
        float minDeadEyeFloor = ctx.isAtCamp ? playerConfig.campedDeadEyeFloor : 0.0f;

        targetDeadEye = std::max(targetDeadEye, minDeadEyeFloor);

        WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", DeadEyeDecay = " + std::to_string(activeDeadEyeDecay) + ", TargetDeadEye=" + std::to_string(targetDeadEye) + ", MinDeadEyeFloor=" + std::to_string(minDeadEyeFloor));            
        WriteLog(LogLevel::Standard, "Player DeadEye Sync Tick: DeadEye=" + std::to_string(targetDeadEye));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), static_cast<int>(targetDeadEye));
    }
}

void ProcessHorseSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, 
    const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig, const CoreConfigSettings& coreConfig) {
    auto [horsePed, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);

	if (!hasHorse)
    {
		WriteLog(LogLevel::Standard, "No active horse detected for the player. Skipping horse core simulation.");
        return;
    }

    bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Health));
    bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Stamina));

    float currentHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health)));
    float currentStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina)));

    WriteLog(LogLevel::Verbose, "Horse Core Simulation: isHealthGold = " + std::to_string(isHorseHealthGold) + ", isStaminaGold = " + std::to_string(isHorseStaminaGold));

	// Health Core Decay Logic (only applies if the horse's health core is not golden)
    if (!isHorseHealthGold) {

        float activeHealthDecay = horseConfig.baseHealthDecay * CalculateHorseHealthDecayMultiplier(horsePed, playerConfig, ctx.isAtCamp);

		float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
		float minHealthFloor = ctx.isAtCamp ? horseConfig.campedHealthFloor : 0.0f; //the player is semi restrained (jail/camp/bath), so we apply the horse's restrained floor to prevent full depletion

        targetHealth = std::max(targetHealth, minHealthFloor);

        WriteLog(LogLevel::Verbose, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), (int)targetHealth);
    }

	// Stamina Core Decay Logic (only applies if the horse's stamina core is not golden)
    if (!isHorseStaminaGold) {
        float targetStamina = currentStamina;

        // If the horse is physically hitched, freeze its stamina completely!
		// Apply a small bonus if the horse is well-fed or health is golden to reward the player for good care
        // This rewards the player for using hitching posts around the world.
        
        if (g_HorseManager.IsHorseHitched(coreConfig)) {
            WriteLog(LogLevel::Standard, "Active Horse is securely hitched. Freezing stamina core decay completely.");

            // Give a little bonus to stamina is horse is well fed
            if (isHorseHealthGold || currentHealth >= HORSE_HITCHED_HEALTH_REQ) {
				float reward = (HORSE_HITCHED_REWARD_PER_GAME_HOUR * hoursDelta);
                targetStamina = std::min(currentStamina + reward, 100.0f);

				WriteLog(LogLevel::Verbose, "Horse is well-fed or has golden health. Applying hitched bonus: +" + std::to_string(reward) + " to stamina core. Horse Stamina updated to: " + std::to_string(targetStamina));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
            }
        }
		else if (g_HorseManager.IsHorseBeingLed(coreConfig)) {
			float reward = (HORSE_LEAD_REWARD_PER_GAME_HOUR * hoursDelta);
            targetStamina = std::min(currentStamina + reward, 100.0f);

            WriteLog(LogLevel::Standard, "Active Horse is being led by the player. Applying cached reward: +" + std::to_string(reward) + ". Horse Stamina updated to: " + std::to_string(targetStamina));
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
		}
        else {
            // Normal dynamic movement decay route
            float activeStaminaDecay = horseConfig.baseStaminaDecay * CalculateHorseStaminaDecayMultiplier(horsePed, horseConfig, playerConfig, ctx.isAtCamp);
            
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
        auto [activeHorse, hasHorse] = g_HorseManager.GetActivePlayerHorse(playerPed);

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

	g_MovementTracker.Reset();
    g_HorseManager.ResetHitchedOrLedTracking();

    s_structuresInitialized = false;    

    ClearDevLogCache();
}


void CheckForConfigChange() {
    if (g_ShouldReloadConfig.load(std::memory_order_acquire) || !s_structuresInitialized) {
        // Log an accurate message based on WHY we are filling the cache
        if (!s_structuresInitialized)
            WriteLog(LogLevel::Verbose, "Initializing local simulation configuration cache for the first time...");
        else
            WriteLog(LogLevel::Standard, "Dynamic hot-reload requested. Updating configuration cache...");

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
        WriteLog(LogLevel::Standard, "Fundamental state boundary flipped! Forcing immediate cache transition process.");
        
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
				WriteLog(LogLevel::Standard, "Processing normal core simulation for elapsed hours: " + std::to_string(hoursDelta));

                ProcessPlayerSimulation(playerPed, hoursDelta, context, generalConfig, playerConfig);
                ProcessHorseSimulation(playerPed, hoursDelta, context, playerConfig, horseConfig, coreConfig);

                // Only clear accumulation on successful simulation cycles!
                g_State.accumulatedTimeMs = 0;
				g_MovementTracker.Reset();
				g_HorseManager.ResetHitchedOrLedTracking();
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