#include "script.h"
#include <array>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <sstream>
#include <algorithm>
#include "Logger.h"
#include "Config.h"
#include "ThreadManager.h"

const std::string VERSION = "v0.1-ALPHA";
const int FAST_STATE_CHECK_INTERVAL_MS = 500;

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
};

struct GameplayContext {
    bool freezeActive = false;
    bool isSleeping = false;
    bool isAtCamp = false;
	bool isWeaponWheel = false;
};

SimulationState g_State;

static CoreConfigSettings    s_cachedCoreConfig;
static GeneralConfigSettings s_cachedGeneralConfig;
static PlayerConfigSettings  s_cachedPlayerConfig;
static HorseConfigSettings   s_cachedHorseConfig;
static bool s_structuresInitialized = false;

std::array<Hash, 11> g_SleepScenarios{};
std::array<Hash, 15> g_CampScenarios{};

Hash g_InputOpenWheelMenuHash;
Hash g_BathingScriptHash;
Hash g_JailScriptHash;
Hash g_CustomJailScriptHash;

bool g_WasRestrictedLastTick = false;

Hash GetKey(const char* key) {
    return MISC::GET_HASH_KEY(key);
}

void PrecomputeHashes() {
	// Precompute hashes here to avoid repeated calls to MISC::GET_HASH_KEY during gameplay.

    g_SleepScenarios = {
        GetKey("WORLD_PLAYER_SLEEP_BEDROLL"),
        GetKey("WORLD_PLAYER_SLEEP_BEDROLL_ARTHUR"),
        GetKey("WORLD_PLAYER_SLEEP_GROUND"),
        GetKey("PROP_PLAYER_SLEEP_BED"),
        GetKey("PROP_PLAYER_SLEEP_BED_ARTHUR"),
        GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME"),
        GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME_ARTHUR"),
        GetKey("PROP_PLAYER_SLEEP_TENT_MALE_A"),
        GetKey("PROP_PLAYER_SLEEP_TENT_MALE_A_ARTHUR"),
        GetKey("PROP_PLAYER_SLEEP_A_FRAME_TENT_PLAYER_CAMPS"),
        GetKey("PROP_PLAYER_SLEEP_A_FRAME_TENT_PLAYER_CAMPS_ARTHUR")
    };

    g_CampScenarios = {
        GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL1"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL2"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL3"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_KNEEL4"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SIT"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SQUAT"),
        GetKey("WORLD_PLAYER_DYNAMIC_KNEEL_KNIFE"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SQUAT_MALE_A"),
        GetKey("WORLD_PLAYER_CAMP_FIRE_SIT_MALE_A"),
        GetKey("WORLD_PLAYER_DYNAMIC_CAMP_FIRE_KNEEL_ARTHUR"),
        GetKey("PROP_PLAYER_SLEEP_TENT_A_FRAME"),
        GetKey("PROP_PLAYER_SEAT_CHAIR_PLAYER_CAMP"),
        GetKey("PROP_PLAYER_SEAT_CHAIR_DYNAMIC"),
        GetKey("PROP_PLAYER_SEAT_CHAIR_GENERIC"),
        GetKey("PROP_PLAYER_SEAT_CHAIR_GENERIC_CA")
    };

    // In the standard RDR2 control layout, INPUT_OPEN_WHEEL_MENU is index 24.
	g_InputOpenWheelMenuHash = 24;
	g_BathingScriptHash = GetKey("bathing");
    g_JailScriptHash = GetKey("bounty_jail");
	g_CustomJailScriptHash = GetKey("enhanced_jail"); // don't actuall know the hash, but this is a placeholder for the Enhanced Jail mod if installed

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

bool IsPlayerGettingArrested() {
    bool isBeingArrested = PLAYER::IS_PLAYER_BEING_ARRESTED(PLAYER::PLAYER_ID(), FALSE);

    // Checks if the vanilla jail cell environment script is actively executing
    bool isVanillaJailScriptActive = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_JailScriptHash) > 0);

    // Placeholder for Enhanced Jail. 
    // If the mod isn't installed, this native just safely returns 0 with no error.
    bool isCustomJailScriptActive = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_CustomJailScriptHash) > 0);

    return (isBeingArrested || isVanillaJailScriptActive || isCustomJailScriptActive);
}

GameplayContext EvaluateGameplayState(Ped playerPed, const GeneralConfigSettings& generalConfig) {
    GameplayContext ctx;

    bool isMission = MISC::GET_MISSION_FLAG();
    bool isMinigame = MISC::IS_MINIGAME_IN_PROGRESS();
    bool isFallingOrDead = PED::IS_PED_FALLING(playerPed) || GRAPHICS::ANIMPOSTFX_IS_RUNNING("MissionFail01") ||PLAYER::IS_PLAYER_DEAD(playerPed) || ENTITY::IS_ENTITY_DEAD(playerPed);
    bool isBathing = (SCRIPT::GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH(g_BathingScriptHash) > 0);
    bool isInJailCell = IsPlayerGettingArrested();

	ctx.isSleeping = IsInSleepScenario(playerPed);
	ctx.isAtCamp = IsInCampScenario(playerPed) && !ctx.isSleeping;
    
    ctx.isWeaponWheel = PAD::IS_CONTROL_PRESSED(0, g_InputOpenWheelMenuHash) || PAD::IS_DISABLED_CONTROL_PRESSED(0, g_InputOpenWheelMenuHash);

	// If the player is in a mission or minigame and the config disallows drain, we freeze the decay.
    bool missionBlock = isMission && !generalConfig.allowDrainInMissions;
    bool minigameBlock = isMinigame && !generalConfig.allowDrainInMinigames;

    // camp stays out as we want a customized slowed decay while at camp
    ctx.freezeActive = (missionBlock || minigameBlock || isFallingOrDead || isBathing || ctx.isSleeping);

    // state trackers to watch for true changes across frames
    static bool lastFreezeActive = false;
    static bool lastIsAtCamp = false;
    static bool lastIsSleeping = false;

    if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev) &&
        (ctx.freezeActive != lastFreezeActive || ctx.isAtCamp != lastIsAtCamp || ctx.isSleeping != lastIsSleeping)) {
        WriteLog(LogLevel::Dev, "Gameplay state evaluated: allowDrainInMissions=" + std::to_string(generalConfig.allowDrainInMissions) + ", allowDrainInMinigames=" + std::to_string(generalConfig.allowDrainInMinigames));

        // Update tracking baselines immediately
        lastFreezeActive = ctx.freezeActive;
        lastIsAtCamp = ctx.isAtCamp;
        lastIsSleeping = ctx.isSleeping;

        std::ostringstream message;
        message << std::boolalpha
            << "Gameplay state changed:"
            << "missionBlock = " << missionBlock
            << ", minigameBlock=" << minigameBlock
            << ", isFallingOrDead=" << isFallingOrDead
            << ", isBathing=" << isBathing
            << ", isSleeping=" << ctx.isSleeping
            << ", isAtCamp=" << ctx.isAtCamp
            << ", isFreezeActive=" << ctx.freezeActive
            << ", isWeaponWheel=" << ctx.isWeaponWheel;

        WriteLog(LogLevel::Dev, message.str());
    }

    return ctx;
}


bool HandleStateTransitions(Ped playerPed, const GameplayContext& ctx, int currentTotalMinutes, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {

    if (ctx.freezeActive && !g_State.isRestricted) {
        g_State.isRestricted = true;
        g_State.cachedInGameTimeMinutes = currentTotalMinutes;

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

		if (elapsedMinutes == 0) return true; // fail fast if no time has actually passed

        float elapsedHours = static_cast<float>(elapsedMinutes) / 60.0f;

        if (elapsedHours > 12.0f) {
            elapsedHours = 12.0f;
            WriteLog(LogLevel::Standard, "Macro time jump detected. Capping simulation catch-up window to 12 hours.");
        }

        // Process flat-rate batch decay drops for the player
        int finalHealth = static_cast<int>(MAX(g_State.cachedHealth - (elapsedHours * playerConfig.baseHealthDecay), playerConfig.healthAwakeFloor));
        int finalStamina = static_cast<int>(MAX(g_State.cachedStamina - (elapsedHours * playerConfig.baseStaminaDecay), playerConfig.staminaFloor));
        int finalDeadEye = static_cast<int>(MAX(g_State.cachedDeadEye - (elapsedHours * playerConfig.baseDeadEyeDecay), playerConfig.deadEyeFloor));

        WriteLog(LogLevel::Standard, "Player Core Sync TIME JUMP Tick: Health=" + std::to_string(finalHealth) + ", Stamina=" + std::to_string(finalStamina) + ", DeadEye=" + std::to_string(finalDeadEye));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), finalHealth);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), finalStamina);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), finalDeadEye);

        // Process batch decay drops remotely for your active horse, even if it's tied to a post
        if (g_State.hadHorse) {
            Ped activeHorse = PED::GET_MOUNT(playerPed);
            if (activeHorse == 0) activeHorse = PLAYER::_GET_ACTIVE_HORSE_FOR_PLAYER(playerPed);
            
            if (activeHorse != 0 && ENTITY::DOES_ENTITY_EXIST(activeHorse)) {
                // Since the horse was resting during the time skip, we apply a base passive metabolism drop
                int finalHorseHealth = static_cast<int>(MAX(g_State.cachedHorseHealth - (elapsedHours * horseConfig.baseHealthDecay), horseConfig.healthFloor));
                int finalHorseStamina = static_cast<int>(MAX(g_State.cachedHorseStamina - (elapsedHours * horseConfig.baseStaminaDecay), horseConfig.staminaFloor));

                WriteLog(LogLevel::Standard, "Horse Core Sync TIME JUMP Tick: Health=" + std::to_string(finalHorseHealth) + ", Stamina=" + std::to_string(finalHorseStamina));

                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health), finalHorseHealth);
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), finalHorseStamina);
            }
        }

        return true;
    }

    return false;
}

float CalculateTimescaleDelta(int currentTotalMinutes)
{
    static int lastTickInGameMinutes = 0;
    static bool isInitialised = false;

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
    if (gameMinutesDelta < 0) {
        gameMinutesDelta += 1440;
    }

    lastTickInGameMinutes = currentTotalMinutes;

    // Macro-Skip Handoff Capping
    // If a time skip > 2 hours occurs, this fails fast (returns 0.0f).
    // This allows HandleStateTransitions to seamlessly catch the skip on the very next line.
    if (gameMinutesDelta == 0 || gameMinutesDelta > 120) return 0.0f;

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

    WriteLog(LogLevel::Dev, "Player Core Simulation: isHealthGold = " + std::to_string(isHealthGold) + ", isStaminaGold = " + std::to_string(isStaminaGold) + ", isDeadEyeGold = " + std::to_string(isDeadEyeGold));

    if (!isHealthGold) {
        float activeHealthDecay = playerConfig.baseHealthDecay;

        if (ctx.isAtCamp)
            activeHealthDecay *= playerConfig.campMultiplier;
		else if (ctx.isWeaponWheel)
			activeHealthDecay *= playerConfig.weaponWheelMultiplier;

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
        float minHealthFloor = ctx.isAtCamp || ctx.isWeaponWheel ? playerConfig.healthSleepFloor : 0.0f;
        
        targetHealth = MAX(targetHealth, minHealthFloor);

		WriteLog(LogLevel::Dev, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Player Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), static_cast<int>(targetHealth));
    }

    if (!isStaminaGold) {
        float activeStaminaDecay = playerConfig.baseStaminaDecay; 
        
        if (ctx.isAtCamp)
            activeStaminaDecay *= playerConfig.campMultiplier;
        else if (ctx.isWeaponWheel)
            activeStaminaDecay *= playerConfig.weaponWheelMultiplier;

        float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
        float minStaminaFloor = ctx.isAtCamp || ctx.isWeaponWheel ? playerConfig.staminaFloor : 0.0f;
        targetStamina = MAX(targetStamina, minStaminaFloor);
		
        WriteLog(LogLevel::Dev, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay = " + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor));
		WriteLog(LogLevel::Standard, "Player Core Sync Tick: Stamina=" + std::to_string(targetStamina));
        
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), static_cast<int>(targetStamina));
    }

    if (!isDeadEyeGold) {
        float activeDeadEyeDecay = playerConfig.baseDeadEyeDecay;

        if (ctx.isAtCamp)
            activeDeadEyeDecay *= playerConfig.campMultiplier;
        else if (ctx.isWeaponWheel)
            activeDeadEyeDecay *= playerConfig.weaponWheelMultiplier;
        else if (isNighttime)
            activeDeadEyeDecay *= playerConfig.nightDeadEyeMultiplier;

        float targetDeadEye = currentDeadEye - (activeDeadEyeDecay * hoursDelta);
        float minDeadEyeFloor = ctx.isAtCamp || ctx.isWeaponWheel ? playerConfig.deadEyeFloor : 0.0f;

        targetDeadEye = MAX(targetDeadEye, minDeadEyeFloor);

		WriteLog(LogLevel::Dev, "Player Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", DeadEyeDecay = " + std::to_string(activeDeadEyeDecay) + ", TargetDeadEye=" + std::to_string(targetDeadEye) + ", MinDeadEyeFloor=" + std::to_string(minDeadEyeFloor));
		WriteLog(LogLevel::Standard, "Player Core Sync Tick: DeadEye=" + std::to_string(targetDeadEye));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), static_cast<int>(targetDeadEye));
    }    
}

void ProcessHorseSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const PlayerConfigSettings& playerConfig, const HorseConfigSettings& horseConfig) {
    Ped horsePed = PED::GET_MOUNT(playerPed);

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
        WriteLog(LogLevel::Dev, "Active Horse: Running/Galloping. Applying high stamina multiplier.");
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot) && currentSpeed <= static_cast<float>(HorseSpeed::Gallop)) {
        staminaMultiplier = horseConfig.trotStaminaMultiplier;
        WriteLog(LogLevel::Dev, "Active Horse: Trotting. Applying medium stamina multiplier.");
    }
    else {
        WriteLog(LogLevel::Dev, "Active Horse: Hitched, standing still, or grazing. Applying base decay.");
    }

    WriteLog(LogLevel::Dev, "Horse Core Simulation: isHealthGold = " + std::to_string(isHorseHealthGold) + ", isStaminaGold = " + std::to_string(isHorseStaminaGold));

    if (!isHorseHealthGold) {
        float activeHealthDecay = horseConfig.baseHealthDecay;

		if (ctx.isAtCamp)
			activeHealthDecay *= playerConfig.campMultiplier;
		else if (ctx.isWeaponWheel)
			activeHealthDecay *= playerConfig.weaponWheelMultiplier;

        float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
        float minHealthFloor = ctx.isAtCamp || ctx.isWeaponWheel ? horseConfig.healthFloor : 0.0f;

        targetHealth = MAX(targetHealth, minHealthFloor);

        WriteLog(LogLevel::Dev, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", HealthDecay = " + std::to_string(activeHealthDecay) + ", TargetHealth=" + std::to_string(targetHealth) + ", MinHealthFloor=" + std::to_string(minHealthFloor));
        WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Health=" + std::to_string(targetHealth));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), (int)targetHealth);
    }

    if (!isHorseStaminaGold) {
        float activeStaminaDecay = (horseConfig.baseStaminaDecay * staminaMultiplier);

        if(ctx.isAtCamp)
			activeStaminaDecay *= playerConfig.campMultiplier;
		else if (ctx.isWeaponWheel)
			activeStaminaDecay *= playerConfig.weaponWheelMultiplier;

        float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);


        float minStaminaFloor = ctx.isAtCamp || ctx.isWeaponWheel ? horseConfig.staminaFloor : 0.0f;


        targetStamina = MAX(targetStamina, minStaminaFloor);

        WriteLog(LogLevel::Dev, "Horse Core Simulation: HoursDelta=" + std::to_string(hoursDelta) + ", StaminaDecay = " + std::to_string(activeStaminaDecay) + ", TargetStamina=" + std::to_string(targetStamina) + ", MinStaminaFloor=" + std::to_string(minStaminaFloor) + ", StaminaMultiplier=" + std::to_string(staminaMultiplier));
        WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Stamina=" + std::to_string(targetStamina));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), (int)targetStamina);
    }    
}

void UpdateCoreSimulation() {
    Ped playerPed = PLAYER::PLAYER_PED_ID();

    // SYSTEM LEVEL RESET GUARD (Save File Loads, Reloading, Death Respawning)
    if (DLC::GET_IS_LOADING_SCREEN_ACTIVE() || !ENTITY::DOES_ENTITY_EXIST(playerPed)) {
        if (g_State.isRestricted || g_State.cachedInGameTimeMinutes != 0) {
            WriteLog(LogLevel::Dev, "System Load/Reload event detected. Flushing simulation cache safely.");
            g_State.isRestricted = false;
            g_State.cachedInGameTimeMinutes = 0;
            g_State.hadHorse = false;
            g_State.accumulatedTimeMs = 0;
        }
        
        return;
    }

    // Death Guard - Freezes without accumulating decay or processing unexpected jump metrics
    if (PLAYER::IS_PLAYER_DEAD(PLAYER::PLAYER_ID())) {
        g_State.isRestricted = false;
        g_State.cachedInGameTimeMinutes = 0;
        g_State.hadHorse = false;
        g_State.accumulatedTimeMs = 0;

        return;
    }

    if (g_ShouldReloadConfig.load(std::memory_order_acquire) || !s_structuresInitialized) {

        std::lock_guard<std::mutex> lock(g_ConfigMutex);

        s_cachedCoreConfig = g_CoreConfig;
        s_cachedGeneralConfig = g_GeneralConfig;
        s_cachedPlayerConfig = g_PlayerConfig;
        s_cachedHorseConfig = g_HorseConfig;

        s_structuresInitialized = true;
        g_ShouldReloadConfig.store(false, std::memory_order_release);
    }

    CoreConfigSettings    coreConfig = s_cachedCoreConfig;
    GeneralConfigSettings generalConfig = s_cachedGeneralConfig;
    PlayerConfigSettings  playerConfig = s_cachedPlayerConfig;
    HorseConfigSettings   horseConfig = s_cachedHorseConfig;

    GameplayContext context = EvaluateGameplayState(playerPed, generalConfig);
    int currentTotalMinutes = GetTotalInGameMinutes();

    // FAST-PATH BOUNDARY TRANSITIONS (Forcing immediate responses to missions or travel updates)
    if (context.freezeActive != g_WasRestrictedLastTick) {
        WriteLog(LogLevel::Dev, "Fundamental state boundary flipped! Forcing immediate cache transition process.");
        
        HandleStateTransitions(playerPed, context, currentTotalMinutes, playerConfig, horseConfig);

        g_WasRestrictedLastTick = context.freezeActive;
        g_State.accumulatedTimeMs = 0;
    }
    // SLOW-PATH APP DECAY SIMULATION (Accumulates interval smoothly over normal open world play)
    else if (!context.freezeActive) {
        g_State.accumulatedTimeMs += FAST_STATE_CHECK_INTERVAL_MS;

        if (g_State.accumulatedTimeMs >= coreConfig.mainLoopTickIntervalMs) {
            float hoursDelta = CalculateTimescaleDelta(currentTotalMinutes);

            // If time actually moved forward in the engine space, process simulation and reset
            if (hoursDelta > 0.0f && hoursDelta < 24.0f) {
                ProcessPlayerSimulation(playerPed, hoursDelta, context, generalConfig, playerConfig);
                ProcessHorseSimulation(playerPed, hoursDelta, context, playerConfig, horseConfig);

                // Only clear accumulation on successful simulation cycles!
                g_State.accumulatedTimeMs = 0;
            }
            else if (hoursDelta == 0.0f) {
                // The real-world interval passed, but the in-game integer clock hasn't ticked up yet.
                // Do NOT reset accumulatedTimeMs. Let it retry on the next 500ms loop pass.
                WriteLog(LogLevel::Dev, "Interval boundary hit, but game engine clock hasn't rolled to a new integer minute. Holding accumulation cache.");
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
        scriptWait(FAST_STATE_CHECK_INTERVAL_MS);
    }
}