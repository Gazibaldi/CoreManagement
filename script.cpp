#define NOMINMAX

#include <array>
#include <string>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "script.h"
#include "Logger.h"
#include "Config.h"
#include "ThreadManager.h"

enum class CoreIndex : int { Health = 0, Stamina = 1, DeadEye = 2 };
enum class HorseSpeed : int { Trot = 2, Gallop = 5 };

struct SimulationState {
    bool isRestricted = false;
    
    int cachedInGameTimeMinutes = 0;
	int accumulatedTimeMs = 0;
    
    float cachedHealth = 100.0f;
    float cachedStamina = 100.0f;
    float cachedDeadEye = 100.0f;
    float cachedHorseHealth = 100.0f;
    float cachedHorseStamina = 100.0f;
    
    bool hadHorse = false;

    bool wasSleepingDuringRestriction = false;
    bool wasAtCampDuringRestriction = false;
};

struct GameplayContext {
    bool freezeActive = false;
    bool isSleeping = false;
    bool isAtCamp = false;
	bool isJailed = false;
	int bathingState = 0;
};

SimulationState g_State;

static CoreConfigSettings    s_cachedCoreConfig;
static GeneralConfigSettings s_cachedGeneralConfig;
static PlayerConfigSettings  s_cachedPlayerConfig;
static HorseConfigSettings   s_cachedHorseConfig;
static bool s_structuresInitialized = false;

std::array<Hash, 11> g_SleepScenarios{};
std::array<Hash, 15> g_CampScenarios{};

Hash g_JailScriptHash;
Hash g_CustomJailScriptHash;

Hash g_PokerScriptHash;
Hash g_BlackjackScriptHash;
Hash g_DominoesScriptHash;
Hash g_FiveFingerFilletScriptHash;

Hash g_BathingScriptHash;
Hash g_BathMaidScriptHash;

bool g_WasRestrictedLastTick = false;

Hash GetKey(const char* key) {
    return MISC::GET_HASH_KEY(key);
}

void PrecomputeHashes() {
	// Precompute hashes here to avoid repeated calls to MISC::GET_HASH_KEY during gameplay.

    g_SleepScenarios = {
        GetKey("WORLD_PLAYER_SLEEP_BEDROLL"),
        GetKey("WORLD_PLAYER_SLEEP_BEDROLL_ARTHUR"),
        GetKey("WORLD_PLAYER_SLEEP_BEDROLL_JOHN"),
        GetKey("WORLD_PLAYER_SLEEP_GROUND"),
        GetKey("PROP_PLAYER_SLEEP_BED"),
        GetKey("PROP_PLAYER_SLEEP_BED_ARTHUR"),
        GetKey("PROP_PLAYER_SLEEP_BED_JOHN"),
        GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME"),
        GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_ARTHUR"),
        GetKey("PROP_CAMP_SLEEP_BEDROLL")
    };

    g_CampScenarios = {
        GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL1"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL2"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SIT"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_GROUND"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SQUAT"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_CRAFT"),
        GetKey("WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_ARTHUR")
    };

    g_BathingScriptHash = GetKey("bathing");
    g_BathingScriptHash = GetKey("bath_maid");

    g_JailScriptHash = GetKey("bounty_jail");
	g_CustomJailScriptHash = GetKey("enhanced_jail"); // don't actually know the hash, but this is a placeholder for the Enhanced Jail mod if installed

	g_PokerScriptHash = GetKey("poker_core");
	g_BlackjackScriptHash = GetKey("blackjack_core");
	g_DominoesScriptHash = GetKey("dominoes_core");
	g_FiveFingerFilletScriptHash = GetKey("five_finger_fillet");
    
	// additional hashes can be precomputed here if needed
}

int GetTotalInGameMinutes() {
    return (CLOCK::GET_CLOCK_HOURS() * 60) + CLOCK::GET_CLOCK_MINUTES();
}

bool IsInSleepScenario(Ped playerPed) {
    Hash activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);
    if (activeScenario == 0) return false;

    return std::find(g_SleepScenarios.begin(), g_SleepScenarios.end(), activeScenario) != g_SleepScenarios.end();
}

bool IsInCampScenario(Ped playerPed) {
    Hash activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);
    if (activeScenario == 0) return false;

    return std::find(g_CampScenarios.begin(), g_CampScenarios.end(), activeScenario) != g_CampScenarios.end();
}

bool IsPlayerArrested() {
    bool isBeingArrested = PLAYER::IS_PLAYER_BEING_ARRESTED(PLAYER::PLAYER_ID(), FALSE);

    // Checks if the vanilla jail cell environment script is actively executing
    bool isVanillaJailScriptActive = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_JailScriptHash) > 0);

    // Placeholder for Enhanced Jail. 
    // If the mod isn't installed, this native just safely returns 0 with no error.
    bool isCustomJailScriptActive = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_CustomJailScriptHash) > 0);

    return (isBeingArrested || isVanillaJailScriptActive || isCustomJailScriptActive);
}

bool IsPlayerPlayingTableMinigame() {
    return (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_PokerScriptHash) > 0) ||
    (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_BlackjackScriptHash) > 0) ||
    (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_DominoesScriptHash) > 0) ||
    (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_FiveFingerFilletScriptHash) > 0);
}

int GetPlayerBathingState() {
    bool isMasterBathThreadRunning = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_BathingScriptHash) > 0);

    if (isMasterBathThreadRunning) {
        bool isMaidActive = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_BathMaidScriptHash) > 0);
        return (isMaidActive ? 2 : 1); // 2 = Deluxe Service, 1 = Standard Self-Wash
    }
   
    return 0;
}

GameplayContext EvaluateGameplayState(Ped playerPed, const GeneralConfigSettings& generalConfig) {
    GameplayContext ctx;

    bool isMission = MISC::GET_MISSION_FLAG();
    bool isMinigame = IsPlayerPlayingTableMinigame();
    bool isFallingOrGoingToDie = PED::IS_PED_FALLING(playerPed) || GRAPHICS::ANIMPOSTFX_IS_RUNNING("MissionFail01");
    
    ctx.bathingState = GetPlayerBathingState();
    ctx.isJailed = IsPlayerArrested();
	ctx.isSleeping = IsInSleepScenario(playerPed);
	ctx.isAtCamp = IsInCampScenario(playerPed) && !ctx.isSleeping;
    
    // If the player is in a mission or minigame and the config disallows drain, we freeze the decay.
    bool missionBlock = isMission && !generalConfig.allowDrainInMissions;
    bool minigameBlock = isMinigame && !generalConfig.allowDrainInMinigames;

    // camp/jailed/bathing stays out as we want a customized slowed decay in those cases
    ctx.freezeActive = (missionBlock || minigameBlock || isFallingOrGoingToDie || ctx.isSleeping);

	// state trackers to watch for true changes across frames
    static bool lastFreezeActive = false;
    static bool lastIsAtCamp = false;
    static bool lastIsSleeping = false;
	static bool lastIsJailed = false;
	static int lastBathingState = 0;

    if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Verbose) &&
        (ctx.freezeActive != lastFreezeActive || ctx.isAtCamp != lastIsAtCamp || ctx.isSleeping != lastIsSleeping || ctx.isJailed != lastIsJailed || ctx.bathingState != lastBathingState)) {
        WriteLog(LogLevel::Verbose, "Gameplay state evaluated: allowDrainInMissions=" + std::to_string(generalConfig.allowDrainInMissions) + ", allowDrainInMinigames=" + std::to_string(generalConfig.allowDrainInMinigames));

        // Update tracking baselines immediately
        lastFreezeActive = ctx.freezeActive;
        lastIsAtCamp = ctx.isAtCamp;
        lastIsSleeping = ctx.isSleeping;
        lastIsJailed = ctx.isJailed;
        lastBathingState = ctx.bathingState;

        std::ostringstream message;
        message << std::boolalpha
            << "Gameplay state changed:"
            << "missionBlock = " << missionBlock
            << ", minigameBlock=" << minigameBlock
            << ", isFallingOrGoingToDie=" << isFallingOrGoingToDie
            << ", bathingState=" << ctx.bathingState
            << ", isSleeping=" << ctx.isSleeping
            << ", isAtCamp=" << ctx.isAtCamp
            << ", isJailed=" << ctx.isJailed
            << ", isFreezeActive=" << ctx.freezeActive;

        WriteLog(LogLevel::Verbose, message.str());
    }    

    return ctx;
}

float CalculateStaminaSleepReplenish(float currentStamina, float elapsedHours,bool isNighttime, bool isHotelRoom) {
    // Standard linear scaling: 10 points per hour of sleep
    float uplift = elapsedHours * 10.0f;

    // POOR SLEEP PENALTY (Less than 8 Hours or daytime and in the wilderness)
	// If you don't get a minimum of 8 hours of rest, your maximum reward capacity is capped hard at 40 points (20 if it's daytime and you're not in a hotel room).
    if (elapsedHours < 8.0f) {
        float cap = (isNighttime || isHotelRoom) ? 40.0f : 20.0f;
        uplift = std::min(uplift, cap);
        WriteLog(LogLevel::Verbose, "Short sleep session detected (< 8 hours). Restorative stamina uplift capped at 40 points.");
    }

	// DAYTIME WILDERNESS SLEEP PENALTY (8+ Hours but not in a hotel room)
	// If you sleep for 8 or more hours but it's daytime and you're not in a hotel room, your maximum reward capacity is capped hard at 50 points.
	if (elapsedHours >= 8.0f && (!isNighttime && !isHotelRoom)) {
        uplift = std::min(uplift, 50.0f);
		WriteLog(LogLevel::Verbose, "Daytime wilderness sleep detected. Stamina uplift capped at 50 points.");
	}

    float finalStamina = currentStamina + uplift;
    return std::min(finalStamina, 100.0f); // Standard full core ceiling
}

float CalculateDeadEyeSleepReplenish(float currentDeadEye, float elapsedHours, bool isNighttime, bool isHotelRoom) {
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

    // THE OVER-SLEEP UNFOCUS ROUTE (10+ Hours)
    if (elapsedHours >= 10.0f) {
		// If current dead eye is higher than the fatigue boundary, apply the 15-point reduction penalty (10 points if in a hotel room)
        if (currentDeadEye > fatigueCap) {
			WriteLog(LogLevel::Verbose, "Over-sleep detected. Dead Eye reduced by 15 points (10 if in a hotel room).");
            return std::max(currentDeadEye - (isHotelRoom ? 10.0f : 15.0f), fatigueCap);
        }

        return fatigueCap;
    }

    // THE REGULAR SLEEP ROUTE (Less than 10 Hours)
    // If your starting Dead Eye is already higher than the focus cap, leave it completely untouched!
    if (currentDeadEye >= focusCap)
        return currentDeadEye;

    float uplift = elapsedHours * 5.0f;
    float finalDeadEye = currentDeadEye + uplift;

    return std::min(finalDeadEye, focusCap);
}

void ApplyBatchTimeSkipDecay(
    Ped playerPed, int elapsedMinutes, float startHealth, float startStamina, float startDeadEye, 
    float startHorseHealth, float startHorseStamina, bool processHorse, bool wasSleeping,
	const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {

    float elapsedHours = static_cast<float>(elapsedMinutes) / 60.0f;
    if (elapsedHours > 12.0f) elapsedHours = 12.0f;

    bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
    bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
    bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    if (!isHealthGold) {
        int finalHealth = static_cast<int>(std::max(startHealth - (elapsedHours * playerConfig.baseHealthDecay),
            wasSleeping ? playerConfig.healthTimeSkipFloor : playerConfig.restrainedHealthFloor));

		WriteLog(LogLevel::Standard, "Player time-skipped. Health decayed to: " + std::to_string(finalHealth));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
    }

    if (wasSleeping) {
        // Gold cores are immune to sleep cap constraints, they remain full
        if (!isStaminaGold) {
            int currentHour = CLOCK::GET_CLOCK_HOURS();
            bool isNighttime = (currentHour >= generalConfig.nightStartHour || currentHour < generalConfig.nightEndHour);
            bool isHotelRoom = !g_State.wasAtCampDuringRestriction;

			// SPECIAL GOLDEN STAMINA CORE REWARD (8-12 Hours in a Hotel Room at Night)
            if (isHotelRoom && isNighttime && elapsedHours >= 8.0f && elapsedHours <= 12.0f) {
				
				WriteLog(LogLevel::Standard, "Player slept 8-12 hours in a hotel room at night. Stamina core is now golden and fully fortified.");
				ATTRIBUTE::ENABLE_ATTRIBUTE_OVERPOWER(playerPed, static_cast<int>(CoreIndex::Stamina), 100.0f, TRUE);
            }
			// REGULAR SLEEP REPLENISHMENT ROUTE (< 8 or > 12 Hours and NOT in a Hotel Room at Night)
            else {

                int finalStamina = static_cast<int>(CalculateStaminaSleepReplenish(startStamina, elapsedHours, isNighttime, isHotelRoom));

                WriteLog(LogLevel::Standard, "Player slept. Stamina replenished to: " + std::to_string(finalStamina));
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
            }
        }
    }
    else if (!isStaminaGold) {
        // Standard macro transit decay (Trains/Stagecoaches)
        int finalStamina = static_cast<int>(std::max(startStamina - (elapsedHours * playerConfig.baseStaminaDecay), playerConfig.restrainedStaminaFloor));

		WriteLog(LogLevel::Standard, "Player time-skipped. Stamina decayed to: " + std::to_string(finalStamina));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
    }

    if (wasSleeping) {
        int currentHour = CLOCK::GET_CLOCK_HOURS();
        bool isNighttime = (currentHour >= generalConfig.nightStartHour || currentHour < generalConfig.nightEndHour);
        bool isHotelRoom = !g_State.wasAtCampDuringRestriction;

        // OVER-SLEEP PUNISHMENT INTERCEPT (10+ Hours)
        if (elapsedHours >= 10.0f) {
            if (isDeadEyeGold) {
                // Forcefully break and remove the golden core overlay state!
                // Passing a value below 100 to a gold core natively forces RDR2 to strip the gold status away instantly.
                WriteLog(LogLevel::Standard, "Player over-slept with a Golden Dead Eye Core. Revoking gold fortification status due to fatigue.");
            }

            int finalDeadEye = static_cast<int>(CalculateDeadEyeSleepReplenish(startDeadEye, elapsedHours, isNighttime, isHotelRoom));

			WriteLog(LogLevel::Standard, "Player overslept. Dead Eye adjusted to: " + std::to_string(finalDeadEye));
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
        }
        // REGULAR SLEEP ROUTE (< 10 Hours)
        else if (!isDeadEyeGold) {
            // Standard sleep replenishment only runs if the core isn't already golden
            int finalDeadEye = static_cast<int>(CalculateDeadEyeSleepReplenish(startDeadEye, elapsedHours, isNighttime, isHotelRoom));
            
            WriteLog(LogLevel::Standard, "Player slept. Dead Eye adjusted to: " + std::to_string(finalDeadEye));
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
        }
    }
    else if (!isDeadEyeGold) {
        // Standard macro transit decay (Trains/Stagecoaches)
        int finalDeadEye = static_cast<int>(std::max(startDeadEye - (elapsedHours * playerConfig.baseDeadEyeDecay), playerConfig.restrainedDeadEyeFloor));

		WriteLog(LogLevel::Standard, "Player time-skipped. Dead Eye decayed to: " + std::to_string(finalDeadEye));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);
    }

    // Compute Horse Core Reductions
    if (processHorse) {
        Ped activeHorse = PED::GET_MOUNT(playerPed);
        if (activeHorse == 0) activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(playerPed);

        if (activeHorse != 0 && ENTITY::DOES_ENTITY_EXIST(activeHorse) && !ENTITY::IS_ENTITY_DEAD(activeHorse)) {
            int finalHorseHealth = static_cast<int>(std::max(startHorseHealth - (elapsedHours * horseConfig.baseHealthDecay), horseConfig.restrainedHealthFloor));
            int finalHorseStamina = static_cast<int>(std::max(startHorseStamina - (elapsedHours * horseConfig.baseStaminaDecay), horseConfig.restrainedStaminaFloor));

            WriteLog(LogLevel::Standard, "Horse Batch Sync: Health=" + std::to_string(finalHorseHealth) + ", Stamina=" + std::to_string(finalHorseStamina));

            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health), finalHorseHealth);
            ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), finalHorseStamina);
        }
    }
}

bool HandleStateTransitions(Ped playerPed, const GameplayContext& ctx, int currentTotalMinutes, const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {

    if (ctx.freezeActive && !g_State.isRestricted) {
        g_State.isRestricted = true;
        g_State.cachedInGameTimeMinutes = currentTotalMinutes;

        // Lock the sleep/camp context down safely right here!
        g_State.wasSleepingDuringRestriction = ctx.isSleeping;
        g_State.wasAtCampDuringRestriction = ctx.isAtCamp;

        // Cache player base structures
        g_State.cachedHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
        g_State.cachedStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
        g_State.cachedDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));

		// Cache horse base structures if the player is mounted or we have an active mount in the world
        Ped activeHorse = PED::GET_MOUNT(playerPed);
        if (activeHorse == 0) activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(playerPed);

        if (activeHorse != 0 && ENTITY::DOES_ENTITY_EXIST(activeHorse)) {
            g_State.hadHorse = true;
            g_State.cachedHorseHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health)));
            g_State.cachedHorseStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina)));
            WriteLog(LogLevel::Standard, "Entering restricted state. Cached player data and active horse tracking metrics remotely.");
        }
        else {
            g_State.hadHorse = false;
            WriteLog(LogLevel::Standard, "Entering restricted state. Cached player data (No active world mount detected).");
        }

        return true;
    }

    // Leaving a restricted / fast-forward state (Run Batch Catch-up Calculations)
    if (!ctx.freezeActive && g_State.isRestricted) {
        g_State.isRestricted = false;
        int elapsedMinutes = currentTotalMinutes - g_State.cachedInGameTimeMinutes;

        if (elapsedMinutes < 0)
            elapsedMinutes += 1440; // Self-corrects midnight rollover cleanly

        WriteLog(LogLevel::Standard, "Exiting restricted state. Total elapsed game minutes: " + std::to_string(elapsedMinutes));

        if (elapsedMinutes > 0)
            // Uses the HISTORICAL frozen values from the cache
            ApplyBatchTimeSkipDecay(playerPed, elapsedMinutes,
                g_State.cachedHealth, g_State.cachedStamina, g_State.cachedDeadEye,
                g_State.cachedHorseHealth, g_State.cachedHorseStamina, g_State.hadHorse,
                g_State.wasSleepingDuringRestriction, generalConfig, playerConfig, horseConfig);

        g_State.wasSleepingDuringRestriction = false;

        return true;
    }

    return false;
}

float CalculateTimescaleDelta(int currentTotalMinutes, bool forceReset = false)
{
    static int lastTickInGameMinutes = 0;
    static bool isInitialised = false;

    // Hard reset hook triggered by upstream macro guards
    if (forceReset) {
        isInitialised = false;
        lastTickInGameMinutes = 0;
        return 0.0f;
    }

    // Safe Initialization (Fails fast on frame 1 without breaking midnight boots)
    if (!isInitialised) {
        lastTickInGameMinutes = currentTotalMinutes;
        isInitialised = true;
        return 0.0f;
    }

    // Fail Fast Optimization (Exits instantly on identical game-time ticks)
    if (currentTotalMinutes == lastTickInGameMinutes) return 0.0f;

    int gameMinutesDelta = currentTotalMinutes - lastTickInGameMinutes;

    // Simple Midnight Rollover Correction
    // If the clock rolled backwards (e.g., from 1439 down to 5 mins), 
    // adding 1440 perfectly restores the true forward time delta.
    if (gameMinutesDelta < 0)
        gameMinutesDelta += 1440;

    // Latch the time baseline immediately
    int previousMinutes = lastTickInGameMinutes;
    lastTickInGameMinutes = currentTotalMinutes;

    if (gameMinutesDelta == 0) return 0.0f;

    // UNMANAGED MACRO JUMP INTERCEPT (Trains, Coaches, Fast Travel)
    // If a time skip > 2 hours happens while the player has normal open-world agency,
	// we return a clear invalid value to signal that UpdateCoreSimulation should take over and process the jump.
    if (gameMinutesDelta > 120) return -1.0f;

    return static_cast<float>(gameMinutesDelta) / 60.0f;
}

bool IsPlayerSemiRestrained(const GameplayContext& ctx, bool includeBathing = true) {
	return (ctx.isAtCamp || ctx.isJailed || (includeBathing && ctx.bathingState > 0));
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

    if (!isHealthGold) {
        float activeHealthDecay = playerConfig.baseHealthDecay;

        if (IsPlayerSemiRestrained(ctx))
            activeHealthDecay *= playerConfig.campJailBathMultiplier;

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
		float minHealthFloor = IsPlayerSemiRestrained(ctx) ? playerConfig.restrainedHealthFloor : 0.0f;  //the player is semi restrained (jail/camp/bath), so we apply the player's restrained floor to prevent full depletion
        
        targetHealth = std::max(targetHealth, minHealthFloor);

		WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Player Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), static_cast<int>(targetHealth));
    }

    if (!isStaminaGold) {
        float targetStamina = currentStamina;

        // Freeze standard drain and apply custom uplift reward
        if (ctx.bathingState > 0) {
            float staminaUplift = 20.0f; // Flat 20 point bonus
            targetStamina = std::min(currentStamina + staminaUplift, 100.0f);
            WriteLog(LogLevel::Verbose, "Bathing Stamina Uplift Reward Applied: " + std::to_string(targetStamina));
        }
        // Normal Open-World or Semi-Restrained Decay Route
        else {
            float activeStaminaDecay = playerConfig.baseStaminaDecay;

            if (IsPlayerSemiRestrained(ctx, false))
                activeStaminaDecay *= playerConfig.campJailBathMultiplier;

            targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
            float minStaminaFloor = IsPlayerSemiRestrained(ctx, false) ? playerConfig.restrainedStaminaFloor : 0.0f;
            targetStamina = std::max(targetStamina, minStaminaFloor);
            
            WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay = " + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor));
        }
        
        WriteLog(LogLevel::Standard, "Player Stamina Sync Tick: Stamina=" + std::to_string(targetStamina));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
    }

    if (!isDeadEyeGold) {
        float targetDeadEye = currentDeadEye;

        // Freeze standard drain and apply custom bath metrics
        if (ctx.bathingState > 0) {
            if (ctx.bathingState == 2) {
                // Deluxe Maid Service completely restores focus
                targetDeadEye = 100.0f;
                WriteLog(LogLevel::Verbose, "Bathing Deluxe Service: Dead Eye completely restored to 100.0f.");
            }
            else if (ctx.bathingState == 1) {
                // Standard Self-Scrub gently steps up but hard-caps at 80.0f
                if (currentDeadEye < 80.0f) {
                    targetDeadEye = std::min(currentDeadEye + 2.0f, 80.0f);
                }
                WriteLog(LogLevel::Verbose, "Bathing Standard Service: Dead Eye capped at a maximum baseline of 80.0f.");
            }
        }
        // Normal Open-World, Nighttime, or Semi-Restrained Decay Route
        else {
            float activeDeadEyeDecay = playerConfig.baseDeadEyeDecay;

            if (IsPlayerSemiRestrained(ctx, false))
                activeDeadEyeDecay *= playerConfig.campJailBathMultiplier;
            else if (isNighttime)
                activeDeadEyeDecay *= playerConfig.nightDeadEyeMultiplier;

            targetDeadEye = currentDeadEye - (activeDeadEyeDecay * hoursDelta);
            float minDeadEyeFloor = IsPlayerSemiRestrained(ctx, false) ? playerConfig.restrainedDeadEyeFloor : 0.0f;

            targetDeadEye = std::max(targetDeadEye, minDeadEyeFloor);

            WriteLog(LogLevel::Verbose, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", DeadEyeDecay = " + std::to_string(activeDeadEyeDecay) + ", TargetDeadEye=" + std::to_string(targetDeadEye) + ", MinDeadEyeFloor=" + std::to_string(minDeadEyeFloor));
        }

        WriteLog(LogLevel::Standard, "Player DeadEye Sync Tick: DeadEye=" + std::to_string(targetDeadEye));
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), static_cast<int>(targetDeadEye));
    }    
}

void ProcessHorseSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {
	Ped horsePed = PED::GET_MOUNT(playerPed); // Attempt to get the horse the player is currently mounted on

    // If not mounted, fall back to tracking the active world horse entity
    if (horsePed == 0) {
        horsePed = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(playerPed);
    }

    // Safety exit: If Arthur doesn't have an active horse spawned, terminate execution
    if (horsePed == 0 ||
        !ENTITY::DOES_ENTITY_EXIST(horsePed) ||
        !ENTITY::IS_ENTITY_A_PED(horsePed) ||
        ENTITY::IS_ENTITY_DEAD(horsePed))
    {
        return;
    }

    bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Health));
    bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Stamina));

    float currentHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health)));
    float currentStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina)));

    // Dynamic Velocity Evaluation (Works whether you are riding it or it's moving ambiently)
    float currentSpeed = ENTITY::GET_ENTITY_SPEED(horsePed);
    float staminaMultiplier = 1.0f;

    // The logic automatically scales down to a base flat rate if the horse is hitched or stabled!
    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        staminaMultiplier = horseConfig.gallopStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Running/Galloping. Applying high stamina multiplier.");
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot) && currentSpeed <= static_cast<float>(HorseSpeed::Gallop)) {
        staminaMultiplier = horseConfig.trotStaminaMultiplier;
        WriteLog(LogLevel::Verbose, "Active Horse: Trotting. Applying medium stamina multiplier.");
    }
    else {
        WriteLog(LogLevel::Verbose, "Active Horse: Hitched, standing still, or grazing. Applying base decay.");
    }

    WriteLog(LogLevel::Verbose, "Horse Core Simulation: isHealthGold = " + std::to_string(isHorseHealthGold) + ", isStaminaGold = " + std::to_string(isHorseStaminaGold));

    if (!isHorseHealthGold) {
        float activeHealthDecay = horseConfig.baseHealthDecay;

		if (IsPlayerSemiRestrained(ctx))
			activeHealthDecay *= playerConfig.campJailBathMultiplier;

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
		float minHealthFloor = IsPlayerSemiRestrained(ctx) ? horseConfig.restrainedHealthFloor : 0.0f; //the player is semi restrained (jail/camp/bath), so we apply the horse's restrained floor to prevent full depletion

        targetHealth = std::max(targetHealth, minHealthFloor);

        WriteLog(LogLevel::Verbose, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), (int)targetHealth);
    }

    if (!isHorseStaminaGold) {
        float activeStaminaDecay = (horseConfig.baseStaminaDecay * staminaMultiplier);

        if(IsPlayerSemiRestrained(ctx))
			activeStaminaDecay *= playerConfig.campJailBathMultiplier;

        float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
        float minStaminaFloor = IsPlayerSemiRestrained(ctx) ? horseConfig.restrainedStaminaFloor : 0.0f;

        targetStamina = std::max(targetStamina, minStaminaFloor);

        WriteLog(LogLevel::Verbose, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay = " + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor) + ", StaminaMultiplier=" + std::to_string(staminaMultiplier));
        WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Stamina=" + std::to_string(targetStamina));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), (int)targetStamina);
    }    
}

void ProcessOpenWorldTimeSkip(Ped playerPed, int currentTotalMinutes, const GeneralConfigSettings& generalConfig, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {
    int elapsedMinutes = currentTotalMinutes - g_State.cachedInGameTimeMinutes;
    if (elapsedMinutes < 0) elapsedMinutes += 1440;

    if (elapsedMinutes > 0) {
        // Reads LIVE data from the engine at the moment of travel
        float liveHealth = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health)));
        float liveStamina = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina)));
        float liveDeadEye = static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye)));

        // We check horse baseline stats live too
        Ped activeHorse = PED::GET_MOUNT(playerPed);
        if (activeHorse == 0) activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(playerPed);
        bool hasHorse = (activeHorse != 0 && ENTITY::DOES_ENTITY_EXIST(activeHorse) && !ENTITY::IS_ENTITY_DEAD(activeHorse));

        float liveHorseHealth = hasHorse ? static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health))) : 0.0f;
        float liveHorseStamina = hasHorse ? static_cast<float>(ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina))) : 0.0f;

        // Route directly into the shared engine (Never sleeping during open-world transits)
        ApplyBatchTimeSkipDecay(playerPed, elapsedMinutes,
            liveHealth, liveStamina, liveDeadEye,
            liveHorseHealth, liveHorseStamina, hasHorse,
            false, generalConfig, playerConfig, horseConfig);
    }

    // Keep the time baseline synchronized
    g_State.cachedInGameTimeMinutes = currentTotalMinutes;
}

void UpdateCoreSimulation() {
    Ped playerPed = PLAYER::PLAYER_PED_ID();

    // SYSTEM LEVEL RESET GUARD (Save File Loads, Reloading, Death Respawning)
    if (DLC::GET_IS_LOADING_SCREEN_ACTIVE() || !ENTITY::DOES_ENTITY_EXIST(playerPed)) {
        if (g_State.isRestricted || g_State.cachedInGameTimeMinutes != 0) {
            WriteLog(LogLevel::Verbose, "System Load/Reload event detected. Flushing simulation cache safely.");
            g_State.isRestricted = false;
            g_State.cachedInGameTimeMinutes = 0;
            g_State.hadHorse = false;
            g_State.accumulatedTimeMs = 0;
            g_State.wasSleepingDuringRestriction = false;
            g_State.wasAtCampDuringRestriction = false;

            CalculateTimescaleDelta(0, true);
        }
        
        return;
    }

    // Death Guard - Freezes without accumulating decay or processing unexpected jump metrics
    if (PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID())) {
        g_State.isRestricted = false;
        g_State.cachedInGameTimeMinutes = 0;
        g_State.hadHorse = false;
        g_State.accumulatedTimeMs = 0;
        g_State.wasSleepingDuringRestriction = false;
        g_State.wasAtCampDuringRestriction = false;

        CalculateTimescaleDelta(0, true);

        return;
    }

    if (g_ShouldReloadConfig.load(std::memory_order_acquire) || !s_structuresInitialized) {

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

	// Cache the global configuration structures for fast-path access in the main loop
    CoreConfigSettings    coreConfig = s_cachedCoreConfig;
    GeneralConfigSettings generalConfig = s_cachedGeneralConfig;
    PlayerConfigSettings  playerConfig = s_cachedPlayerConfig;
    HorseConfigSettings   horseConfig = s_cachedHorseConfig;

    GameplayContext context = EvaluateGameplayState(playerPed, generalConfig);
    int currentTotalMinutes = GetTotalInGameMinutes();

    // FAST-PATH BOUNDARY TRANSITIONS (Forcing immediate responses to missions or travel updates)
    if (context.freezeActive != g_WasRestrictedLastTick) {
        WriteLog(LogLevel::Verbose, "Fundamental state boundary flipped! Forcing immediate cache transition process.");
        
        HandleStateTransitions(playerPed, context, currentTotalMinutes, generalConfig, playerConfig, horseConfig);

        g_WasRestrictedLastTick = context.freezeActive;
        g_State.accumulatedTimeMs = 0;
    }
    // SLOW-PATH APP DECAY SIMULATION (Accumulates interval smoothly over normal open world play)
    else if (!context.freezeActive) {
        g_State.accumulatedTimeMs += g_stateChangeTickIntervalMs;

        if (g_State.accumulatedTimeMs >= coreConfig.coreDrainTickIntervalMs) {
            float hoursDelta = CalculateTimescaleDelta(currentTotalMinutes);

            if (hoursDelta == 0.0f) {
                // The real-world interval passed, but the in-game integer clock hasn't ticked up yet.
                // Do NOT reset accumulatedTimeMs. Let it retry on the next 500ms loop pass.
                WriteLog(LogLevel::Verbose, "Interval boundary hit, but game engine clock hasn't rolled to a new integer minute. Holding accumulation cache.");
            }
            // If time actually moved forward in the engine space, process simulation and reset
            else if (hoursDelta > 0.0f && hoursDelta < 24.0f) {
                ProcessPlayerSimulation(playerPed, hoursDelta, context, generalConfig, playerConfig);
                ProcessHorseSimulation(playerPed, hoursDelta, context, playerConfig, horseConfig);

                // Only clear accumulation on successful simulation cycles!
                g_State.accumulatedTimeMs = 0;
            } 
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
    g_IniPath = ".\\CoreManagement.ini";
    g_LogPath = ".\\CoreManagement.log";

    // Wipe the log file on each script load to avoid excessive file growth
	std::ofstream(g_LogPath, std::ios::out | std::ios::trunc).close();

    StartAsyncLogger();

    LoadConfiguration();
    PrecomputeHashes();

    WriteLog(LogLevel::Standard, "==== Core Drain Management Simulation Engine Initialised Successfully [" + VERSION + "] ====");

	// Start the INI watcher thread to monitor for configuration changes
    // Let the global manager handle spinning up the background watcher thread
    g_ThreadManager.Initialize();
        
    while (true) {
        UpdateCoreSimulation();
        scriptWait(g_stateChangeTickIntervalMs);
    }
}