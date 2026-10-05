#include "script.h"
#include <windows.h>
#include <array>
#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "Logger.h"
#include "Config.h"

#ifndef CLAMP_MAX
#define CLAMP_MAX(value, floorLimit) ((value) > (floorLimit) ? (value) : (floorLimit))
#endif // !CLAMP_MAX

#ifndef CLAMP_MIN
#define CLAMP_MIN(value, ceilingLimit) ((value) < (ceilingLimit) ? (value) : (ceilingLimit))
#endif // !CLAMP_MIN

const std::string VERSION = "v0.1-ALPHA";

enum class CoreIndex : int { Health = 0, Stamina = 1, DeadEye = 2 };
enum class HorseSpeed : int { Trot = 2, Gallop = 5 };

struct GeneralConfigSettings {
    int nightStartHour = 19;
    int nightEndHour = 7;

    bool allowDrainInMissions = false;
    bool allowDrainInMinigames = false;

    LogLevel currentLogLevel = LogLevel::Standard;
};

struct PlayerConfigSettings {
    float baseHealthDecay = 3.5f;
    float baseStaminaDecay = 5.5f;
    float baseDeadEyeDecay = 4.16f;
    float sleepHealthMultiplier = 0.5f;
    float nightDeadEyeMultiplier = 1.5f;
	float campMultiplier = 0.25f;
    float weaponWheelMultiplier = 0.05f;
    float healthSleepFloor = 15.0f;
    float healthAwakeFloor = 1.0f;
    float staminaFloor = 1.0f;
    float deadEyeFloor = 1.0f;
};

struct HorseConfigSettings {
    float baseHealthDecay = 3.5f;
    float baseStaminaDecay = 5.5f;
    float trotStaminaMultiplier = 1.3f;
    float gallopStaminaMultiplier = 4.5f;
    float healthFloor = 5.0f;
    float staminaFloor = 1.0f;
};

struct SimulationState {
    bool isRestricted = false;
    
    int cachedInGameTimeMinutes = 0;
    
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


CoreConfigSettings g_CoreConfig;
GeneralConfigSettings g_GeneralConfig;
PlayerConfigSettings g_PlayerConfig;
HorseConfigSettings g_HorseConfig;
SimulationState g_State;

std::array<Hash, 11> g_SleepScenarios{};
std::array<Hash, 15> g_CampScenarios{};

std::mutex g_ConfigMutex;
std::atomic<bool> g_ShouldReloadConfig(false);
std::atomic<bool> g_RunWatcherThread(true);
std::atomic<int> g_TickIntervalMs(1000);
std::string g_IniPath = "";

float GetIniFloat(const char* section, const char* key, float defaultValue, const char* filePath) {
    char result[64];
    std::string defStr = std::to_string(defaultValue);
    GetPrivateProfileStringA(section, key, defStr.c_str(), result, sizeof(result), filePath);

    return std::stof(result);
}

int GetIniInt(const char* section, const char* key, int defaultValue, const char* filePath) {
    return GetPrivateProfileIntA(section, key, defaultValue, filePath);
}

bool GetIniBool(const char* section, const char* key, bool defaultValue, const char* filePath) {
    int defaultInt = defaultValue ? 1 : 0;
    int result = GetPrivateProfileIntA(section, key, defaultInt, filePath);

    return result != 0;
}

Hash GetKey(const char* key) {
    return MISC::GET_HASH_KEY(key);
}

void InitializeScenarioHashes() {
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
}

bool DoesFileExist(const std::string& filePath) {
    DWORD fileAttributes = GetFileAttributesA(filePath.c_str());
    return (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

void LoadConfiguration() {

    if(!DoesFileExist(g_IniPath)) {
		WriteLog(LogLevel::Standard, "Configuration file not found at: " + g_IniPath);
		WriteLog(LogLevel::Standard, "Loading default configuration file...");
	}

    std::lock_guard<std::mutex> lock(g_ConfigMutex);
    
	g_CoreConfig.mainLoopTickIntervalMs = CLAMP_MAX(GetIniInt("Performance", "MainLoopTickIntervalMs", 1000, g_IniPath.c_str()), 1000); // minimum 1 second tick interval
	g_CoreConfig.logLevelRaw = CLAMP_MIN(CLAMP_MAX(GetIniInt("Logging", "LogLevel", 1, g_IniPath.c_str()), 0), 2); // clamp between 0 and 2 
    g_GeneralConfig.nightStartHour = CLAMP_MIN(CLAMP_MAX(GetIniInt("General", "NightStartHour", 19, g_IniPath.c_str()), 0), 23); // clamp between 0 and 23
    g_GeneralConfig.nightEndHour = CLAMP_MIN(CLAMP_MAX(GetIniInt("General", "NightEndHour", 7, g_IniPath.c_str()), 0), 23); // clamp between 0 and 23
    g_GeneralConfig.allowDrainInMissions = GetIniBool("General", "AllowDrainInMissions", false, g_IniPath.c_str());
    g_GeneralConfig.allowDrainInMinigames = GetIniBool("General", "AllowDrainInMinigames", false, g_IniPath.c_str());

    g_PlayerConfig.baseHealthDecay = CLAMP_MIN(CLAMP_MAX(GetIniFloat("BaseDecayAwake", "HealthDecayBase", 3.5f, g_IniPath.c_str()), 1.0f), 100.0f);
    g_PlayerConfig.baseStaminaDecay = CLAMP_MIN(CLAMP_MAX(GetIniFloat("BaseDecayAwake", "StaminaDecayBase", 5.5f, g_IniPath.c_str()), 1.0f), 100.0f);
    g_PlayerConfig.baseDeadEyeDecay = CLAMP_MIN(CLAMP_MAX(GetIniFloat("BaseDecayAwake", "DeadEyeDecayBase", 4.16f, g_IniPath.c_str()), 1.0f), 100.0f);
    g_PlayerConfig.sleepHealthMultiplier = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Modifiers", "SleepHealthMultiplier", 0.5f, g_IniPath.c_str()), 0.01f), 10.0f);
    g_PlayerConfig.nightDeadEyeMultiplier = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Modifiers", "NightDeadEyeMultiplier", 1.5f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_PlayerConfig.weaponWheelMultiplier = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Modifiers", "WeaponWheelMultiplier", 0.05f, g_IniPath.c_str()), 0.01f), 10.0f);
    g_PlayerConfig.campMultiplier = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Modifiers", "CampMultiplier", 0.25f, g_IniPath.c_str()), 0.01f), 10.0f);
    g_PlayerConfig.healthSleepFloor = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Floors", "HealthSleepFloor", 15.0f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_PlayerConfig.healthAwakeFloor = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Floors", "HealthAwakeFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_PlayerConfig.staminaFloor = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Floors", "StaminaFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_PlayerConfig.deadEyeFloor = CLAMP_MIN(CLAMP_MAX(GetIniFloat("Floors", "DeadEyeFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);

    g_HorseConfig.baseHealthDecay = CLAMP_MIN(CLAMP_MAX(GetIniFloat("HorseBaseDecay", "HorseHealthDecayBase", 3.5f, g_IniPath.c_str()), 1.0f), 100.0f);
    g_HorseConfig.baseStaminaDecay = CLAMP_MIN(CLAMP_MAX(GetIniFloat("HorseBaseDecay", "HorseStaminaDecayBase", 5.5f, g_IniPath.c_str()), 1.0f), 100.0f);
    g_HorseConfig.trotStaminaMultiplier = CLAMP_MIN(CLAMP_MAX(GetIniFloat("HorseMovementModifiers", "HorseTrotStaminaMultiplier", 1.3f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_HorseConfig.gallopStaminaMultiplier = CLAMP_MIN(CLAMP_MAX(GetIniFloat("HorseMovementModifiers", "HorseGallopStaminaMultiplier", 4.5f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_HorseConfig.healthFloor = CLAMP_MIN(CLAMP_MAX(GetIniFloat("HorseFloors", "HorseHealthFloor", 5.0f, g_IniPath.c_str()), 1.0f), 10.0f);
    g_HorseConfig.staminaFloor = CLAMP_MIN(CLAMP_MAX(GetIniFloat("HorseFloors", "HorseStaminaFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);

    if (g_CoreConfig.logLevelRaw >= 2) g_CoreConfig.currentLogLevel = LogLevel::Dev;
    else if (g_CoreConfig.logLevelRaw == 1) g_CoreConfig.currentLogLevel = LogLevel::Standard;
    else g_CoreConfig.currentLogLevel = LogLevel::Disabled;

    g_TickIntervalMs.store(g_CoreConfig.mainLoopTickIntervalMs);

    if (g_CoreConfig.currentLogLevel == LogLevel::Dev) {
        std::ostringstream table;
        table << std::fixed << std::setprecision(2)
            << "\n\n+------------------------------------+------------+\n"
            << "| Configuration Parameter            |      Value |\n"
            << "+------------------------------------+------------+\n"
            << "| Performance: Tick Interval (ms)    | " << std::setw(10) << g_CoreConfig.mainLoopTickIntervalMs << " |\n"
            << "| Logging: Log Level                 | " << std::setw(10) << static_cast<int>(g_CoreConfig.currentLogLevel) << " |\n"
            << "| General: Night Start Hour          | " << std::setw(10) << g_GeneralConfig.nightStartHour << " |\n"
            << "| General: Night End Hour            | " << std::setw(10) << g_GeneralConfig.nightEndHour << " |\n"
            << "| General: Allow Drain in Missions   | " << std::setw(10) << g_GeneralConfig.allowDrainInMissions << " |\n"
            << "| General: Allow Drain in Minigames  | " << std::setw(10) << g_GeneralConfig.allowDrainInMinigames << " |\n"
            << "| Player Health Base Awake Decay     | " << std::setw(10) << g_PlayerConfig.baseHealthDecay << " |\n"
            << "| Player Stamina Base Awake Decay    | " << std::setw(10) << g_PlayerConfig.baseStaminaDecay << " |\n"
            << "| Player Dead Eye Base Awake Decay   | " << std::setw(10) << g_PlayerConfig.baseDeadEyeDecay << " |\n"
            << "| Player Sleep Health Multiplier     | " << std::setw(10) << g_PlayerConfig.sleepHealthMultiplier << " |\n"
            << "| Player Night Dead Eye Multiplier   | " << std::setw(10) << g_PlayerConfig.nightDeadEyeMultiplier << " |\n"
            << "| Player Weapon Wheel Multiplier     | " << std::setw(10) << g_PlayerConfig.weaponWheelMultiplier << " |\n"
            << "| Player Camp Multiplier             | " << std::setw(10) << g_PlayerConfig.campMultiplier << " |\n"
            << "| Player Health Sleep Floor          | " << std::setw(10) << g_PlayerConfig.healthSleepFloor << " |\n"
            << "| Player Health Awake Floor          | " << std::setw(10) << g_PlayerConfig.healthAwakeFloor << " |\n"
            << "| Player Stamina Floor               | " << std::setw(10) << g_PlayerConfig.staminaFloor << " |\n"
            << "| Player Dead Eye Floor              | " << std::setw(10) << g_PlayerConfig.deadEyeFloor << " |\n"
            << "| Horse Health Base Decay            | " << std::setw(10) << g_HorseConfig.baseHealthDecay << " |\n"
            << "| Horse Stamina Base Decay           | " << std::setw(10) << g_HorseConfig.baseStaminaDecay << " |\n"
            << "| Horse Stamina Trot Multiplier      | " << std::setw(10) << g_HorseConfig.trotStaminaMultiplier << " |\n"
            << "| Horse Stamina Gallop Multiplier    | " << std::setw(10) << g_HorseConfig.gallopStaminaMultiplier << " |\n"
            << "| Horse Health Floor                 | " << std::setw(10) << g_HorseConfig.healthFloor << " |\n"
            << "| Horse Stamina Floor                | " << std::setw(10) << g_HorseConfig.staminaFloor << " |\n"
            << "+------------------------------------+------------+";

        g_ConfigMutex.unlock();
        WriteLog(LogLevel::Dev, "Loading ini configuration from: " + g_IniPath);
        WriteLog(LogLevel::Dev, "Internal Parameter Map Hydrated:" + table.str());
        g_ConfigMutex.lock();
    }
}

void IniWatcherThread() {
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    FILETIME lastWriteTime = { 0, 0 };

    if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
        lastWriteTime = fileData.ftLastWriteTime;
    }

    while (g_RunWatcherThread) {
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
            if (CompareFileTime(&fileData.ftLastWriteTime, &lastWriteTime) > 0) {
                lastWriteTime = fileData.ftLastWriteTime;
                g_ShouldReloadConfig.store(true);
                WriteLog(LogLevel::Standard, "Config change detected. Hot-reload queued.");
            }
        }
    }
}

int GetTotalInGameMinutes() {
    return (CLOCK::GET_CLOCK_HOURS() * 60) + CLOCK::GET_CLOCK_MINUTES();
}

void ProcessHotReload() {
    if (g_ShouldReloadConfig.load()) {
        g_ShouldReloadConfig.store(false);
        LoadConfiguration();
        WriteLog(LogLevel::Standard, "Configuration dynamically reloaded.");
    }
}

bool IsInSleepScenario(Ped playerPed)
{
    Hash activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);

    for (Hash sleepScenario : g_SleepScenarios) {
        if (activeScenario == sleepScenario)
            return true;
    }

    return false;
}

bool IsInCampScenario(Ped playerPed)
{
    Hash activeScenario = PED::_GET_ACTIVE_DYNAMIC_SCENARIO(playerPed);

    for (Hash campScenario : g_CampScenarios) {
        if (activeScenario == campScenario)
            return true;
    }

    return false;
}

GameplayContext EvaluateGameplayState(Ped playerPed, const GeneralConfigSettings& localGeneralConfig) {
    GameplayContext ctx;

	// global query for mission, minigame, or cutscene state
    bool isMission = MISC::GET_MISSION_FLAG();
    bool isMinigame = MISC::IS_MINIGAME_IN_PROGRESS();
    bool isFallingOrDead = PED::IS_PED_FALLING(playerPed) || GRAPHICS::ANIMPOSTFX_IS_RUNNING("MissionFail01") ||PLAYER::IS_PLAYER_DEAD(playerPed) || ENTITY::IS_ENTITY_DEAD(playerPed);
    bool isBathing = MISC::ARE_STRINGS_EQUAL(TASK::GET_TASK_MOVE_NETWORK_STATE(playerPed), "Bathing");

	ctx.isSleeping = IsInSleepScenario(playerPed);
	ctx.isAtCamp = IsInCampScenario(playerPed) && !ctx.isSleeping;
    
    ctx.isWeaponWheel = PAD::IS_CONTROL_PRESSED(0, GetKey("INPUT_OPEN_WHEEL_MENU")) || PAD::IS_DISABLED_CONTROL_PRESSED(0, GetKey("INPUT_OPEN_WHEEL_MENU"));

	// If the player is in a mission or minigame and the config disallows drain, we freeze the decay.
    bool missionBlock = isMission && !localGeneralConfig.allowDrainInMissions;
    bool minigameBlock = isMinigame && !localGeneralConfig.allowDrainInMinigames;

    // camp stays out as we want a customized slowed decay while at camp
    ctx.freezeActive = (missionBlock || minigameBlock || isFallingOrDead || isBathing || ctx.isSleeping);

    if (g_CoreConfig.currentLogLevel == LogLevel::Dev) {
        WriteLog(LogLevel::Dev, "Gameplay state evaluated: allowDrainInMissions=" + std::to_string(localGeneralConfig.allowDrainInMissions) + ", allowDrainInMinigames=" + std::to_string(localGeneralConfig.allowDrainInMinigames));

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


bool HandleStateTransitions(Ped playerPed, const GameplayContext& ctx, int currentTotalMinutes, const PlayerConfigSettings& localPlayerConfig, const HorseConfigSettings& localHorseConfig) {

    if (ctx.freezeActive && !g_State.isRestricted) {
        g_State.isRestricted = true;
        g_State.cachedInGameTimeMinutes = currentTotalMinutes;

        // Cache player base structures
        g_State.cachedHealth = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health));
        g_State.cachedStamina = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina));
        g_State.cachedDeadEye = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye));

		// Cache horse base structures if the player is mounted or we have an active mount in the world
        Ped activeHorse = PED::GET_MOUNT(playerPed);
        if (activeHorse != 0 && ENTITY::DOES_ENTITY_EXIST(activeHorse)) {
            g_State.hadHorse = true;
            g_State.cachedHorseHealth = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health));
            g_State.cachedHorseStamina = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina));
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

        WriteLog(LogLevel::Standard, "Exiting restricted state. Total elapsed game minutes: " + std::to_string(elapsedMinutes));
        
        if (elapsedMinutes <= 0) return true;

        float elapsedHours = (float)elapsedMinutes / 60.0f;

        // Process flat-rate batch decay drops for the player
        float finalHealth = CLAMP_MAX(g_State.cachedHealth - (elapsedHours * localPlayerConfig.baseHealthDecay), localPlayerConfig.healthAwakeFloor);
        float finalStamina = CLAMP_MAX(g_State.cachedStamina - (elapsedHours * localPlayerConfig.baseStaminaDecay), localPlayerConfig.staminaFloor);
        float finalDeadEye = CLAMP_MAX(g_State.cachedDeadEye - (elapsedHours * localPlayerConfig.baseDeadEyeDecay), localPlayerConfig.deadEyeFloor);

        WriteLog(LogLevel::Standard, "Player Core Sync TIME JUMP Tick: Health=" + std::to_string(finalHealth) + ", Stamina=" + std::to_string(finalStamina) + ", DeadEye=" + std::to_string(finalDeadEye));

        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), (int)finalHealth);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), (int)finalStamina);
        ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), (int)finalDeadEye);

        // Process batch decay drops remotely for your active horse, even if it's tied to a post
        if (g_State.hadHorse) {
            Ped activeHorse = PED::GET_MOUNT(playerPed);
            
            if (activeHorse != 0 && ENTITY::DOES_ENTITY_EXIST(activeHorse)) {
                // Since the horse was resting during the time skip, we apply a base passive metabolism drop
                float finalHorseHealth = CLAMP_MAX(g_State.cachedHorseHealth - (elapsedHours * localHorseConfig.baseHealthDecay), localHorseConfig.healthFloor);
                float finalHorseStamina = CLAMP_MAX(g_State.cachedHorseStamina - (elapsedHours * localHorseConfig.baseStaminaDecay), localHorseConfig.staminaFloor);

                WriteLog(LogLevel::Standard, "Horse Core Sync TIME JUMP Tick: Health=" + std::to_string(finalHorseHealth) + ", Stamina=" + std::to_string(finalHorseStamina));

                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Health), (int)finalHorseHealth);
                ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(activeHorse, static_cast<int>(CoreIndex::Stamina), (int)finalHorseStamina);
            }
        }

        return true;
    }

    return false;
}

float CalculateTimescaleDelta(int currentTotalMinutes)
{
    static int lastTickInGameMinutes = 0;

    // Initialize the baseline on your very first execution loop
    if (lastTickInGameMinutes == 0)
    {
        lastTickInGameMinutes = currentTotalMinutes;

        return 0.0f;
    }

    int gameMinutesDelta = currentTotalMinutes - lastTickInGameMinutes;

    // MIDNIGHT WRAP-AROUND GUARD:
    // If the clock just rolled over midnight (e.g., from 1439 mins down to 0 mins),
    // the delta will be negative. Adding 1440 (minutes in a full day) self-corrects the math perfectly.
    if (gameMinutesDelta < 0) gameMinutesDelta += 1440;

    lastTickInGameMinutes = currentTotalMinutes;

    // Boundary check: If a massive skip or scene loading reload just occurred,
    // break execution out so your dedicated HandleStateTransitions function catches it instead.
    if (gameMinutesDelta == 0 || gameMinutesDelta > 120) return 0.0f;

    return (float)gameMinutesDelta / 60.0f;
}

void ProcessPlayerSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const GeneralConfigSettings& localGeneralConfig, const PlayerConfigSettings& localPlayerConfig) {
    int currentHour = CLOCK::GET_CLOCK_HOURS();
    bool isNighttime = (currentHour >= localGeneralConfig.nightStartHour || currentHour < localGeneralConfig.nightEndHour);

	bool isHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Health));
	bool isStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::Stamina));
	bool isDeadEyeGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(playerPed, static_cast<int>(CoreIndex::DeadEye));

    float currentHealth = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health));
    float currentStamina = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina));
    float currentDeadEye = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye));

    float activeHealthDecay = 0.0f;    
    if (!isHealthGold) {
        activeHealthDecay = localPlayerConfig.baseHealthDecay;

        if (ctx.isAtCamp)
            activeHealthDecay *= localPlayerConfig.campMultiplier;
		else if (ctx.isWeaponWheel)
			activeHealthDecay *= localPlayerConfig.weaponWheelMultiplier;
    }

    float activeStaminaDecay = 0.0f;
    if (!isStaminaGold) {
        activeStaminaDecay = localPlayerConfig.baseStaminaDecay; 
        
        if (ctx.isAtCamp)
            activeStaminaDecay *= localPlayerConfig.campMultiplier;
        else if (ctx.isWeaponWheel)
            activeStaminaDecay *= localPlayerConfig.weaponWheelMultiplier;
    }

    float activeDeadEyeDecay = 0.0f;
    if (!isDeadEyeGold) {
        activeDeadEyeDecay = localPlayerConfig.baseDeadEyeDecay;

        if (ctx.isAtCamp)
            activeDeadEyeDecay *= localPlayerConfig.campMultiplier;
        else if (ctx.isWeaponWheel)
            activeDeadEyeDecay *= localPlayerConfig.weaponWheelMultiplier;
        else if (isNighttime)
            activeDeadEyeDecay *= localPlayerConfig.nightDeadEyeMultiplier;
    }

    float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
    float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);
    float targetDeadEye = currentDeadEye - (activeDeadEyeDecay * hoursDelta);

    float minHealthFloor = ctx.isAtCamp || ctx.isWeaponWheel ? localPlayerConfig.healthSleepFloor : 0.0f;
    float minStaminaFloor = ctx.isAtCamp || ctx.isWeaponWheel ? localPlayerConfig.staminaFloor : 0.0f;
    float minDeadEyeFloor = ctx.isAtCamp || ctx.isWeaponWheel ? localPlayerConfig.deadEyeFloor : 0.0f;

    targetHealth = CLAMP_MAX(targetHealth, minHealthFloor);
    targetStamina = CLAMP_MAX(targetStamina, minStaminaFloor);
    targetDeadEye = CLAMP_MAX(targetDeadEye, minDeadEyeFloor);

	WriteLog(LogLevel::Dev, "Player Core Simulation: HealthDecay=" + std::to_string(activeHealthDecay) + ", StaminaDecay=" + std::to_string(activeStaminaDecay) + ", DeadEyeDecay=" + std::to_string(activeDeadEyeDecay) + ", HoursDelta=" + std::to_string(hoursDelta));
	WriteLog(LogLevel::Dev, "Player Core Simulation: CurrentHealth=" + std::to_string(currentHealth) + ", CurrentStamina=" + std::to_string(currentStamina) + ", CurrentDeadEye=" + std::to_string(currentDeadEye));
    WriteLog(LogLevel::Dev, "Player Core Simulation: minHealthFloor=" + std::to_string(minHealthFloor) + ", minStaminaFloor=" + std::to_string(minStaminaFloor) + ", minDeadEyeFloor=" + std::to_string(minDeadEyeFloor));
    WriteLog(LogLevel::Dev, "Player Core Simulation: isHealthGold = " + std::to_string(isHealthGold) + ", isStaminaGold = " + std::to_string(isStaminaGold) + ", isDeadEyeGold = " + std::to_string(isDeadEyeGold));

	WriteLog(LogLevel::Standard, "Player Core Sync Tick: Health=" + std::to_string(targetHealth) + ", Stamina=" + std::to_string(targetStamina) + ", DeadEye=" + std::to_string(targetDeadEye));

    ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Health), (int)targetHealth);
    ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::Stamina), (int)targetStamina);
    ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(playerPed, static_cast<int>(CoreIndex::DeadEye), (int)targetDeadEye);
}

void ProcessHorseSimulation(Ped playerPed, float hoursDelta, const GameplayContext& ctx, const PlayerConfigSettings& localPlayerConfig, const HorseConfigSettings& localHorseConfig) {
    Ped horsePed = PED::GET_MOUNT(playerPed);

    // Safety exit: If Arthur doesn't have an active horse spawned, terminate execution
    if (horsePed == 0 || !ENTITY::DOES_ENTITY_EXIST(horsePed)) return;

    bool isHorseHealthGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Health));
    bool isHorseStaminaGold = ATTRIBUTE::_IS_ATTRIBUTE_CORE_OVERPOWERED(horsePed, static_cast<int>(CoreIndex::Stamina));

    float currentHealth = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health));
    float currentStamina = (float)ATTRIBUTE::_GET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina));

    // Dynamic Velocity Evaluation (Works whether you are riding it or it's moving ambiently)
    float currentSpeed = ENTITY::GET_ENTITY_SPEED(horsePed);
    float staminaMultiplier = 1.0f;

    // The logic automatically scales down to a base flat rate if the horse is hitched or stabled!
    if (currentSpeed > static_cast<float>(HorseSpeed::Gallop)) {
        staminaMultiplier = localHorseConfig.gallopStaminaMultiplier;
        WriteLog(LogLevel::Dev, "Active Horse: Running/Galloping. Applying high stamina multiplier.");
    }
    else if (currentSpeed > static_cast<float>(HorseSpeed::Trot) && currentSpeed <= static_cast<float>(HorseSpeed::Gallop)) {
        staminaMultiplier = localHorseConfig.trotStaminaMultiplier;
        WriteLog(LogLevel::Dev, "Active Horse: Trotting. Applying medium stamina multiplier.");
    }
    else {
        WriteLog(LogLevel::Dev, "Active Horse: Hitched, standing still, or grazing. Applying base decay.");
    }

    float activeHealthDecay = 0.0f;
    if (!isHorseHealthGold) {
        activeHealthDecay = localHorseConfig.baseHealthDecay;

		if (ctx.isAtCamp)
			activeHealthDecay *= localPlayerConfig.campMultiplier;
		else if (ctx.isWeaponWheel)
			activeHealthDecay *= localPlayerConfig.weaponWheelMultiplier;
    }

    float activeStaminaDecay = 0.0f;
    if (!isHorseStaminaGold) {
        activeStaminaDecay = (localHorseConfig.baseStaminaDecay * staminaMultiplier);

        if(ctx.isAtCamp)
			activeStaminaDecay *= localPlayerConfig.campMultiplier;
		else if (ctx.isWeaponWheel)
			activeStaminaDecay *= localPlayerConfig.weaponWheelMultiplier;
    }

    float targetHealth = currentHealth - (activeHealthDecay * hoursDelta);
    float targetStamina = currentStamina - (activeStaminaDecay * hoursDelta);

    float minHealthFloor = ctx.isAtCamp || ctx.isWeaponWheel ? localHorseConfig.healthFloor : 0.0f;
    float minStaminaFloor = ctx.isAtCamp || ctx.isWeaponWheel ? localHorseConfig.staminaFloor : 0.0f;

    targetHealth = CLAMP_MAX(targetHealth, minHealthFloor);
    targetStamina = CLAMP_MAX(targetStamina, minStaminaFloor);

    WriteLog(LogLevel::Dev, "Horse Core Simulation: HealthDecay=" + std::to_string(activeHealthDecay) + ", StaminaDecay=" + std::to_string(activeStaminaDecay) + ", HoursDelta=" + std::to_string(hoursDelta));
    WriteLog(LogLevel::Dev, "Horse Core Simulation: CurrentHealth=" + std::to_string(currentHealth) + ", CurrentStamina=" + std::to_string(currentStamina));
    WriteLog(LogLevel::Dev, "Horse Core Simulation: minHealthFloor=" + std::to_string(minHealthFloor) + ", minStaminaFloor=" + std::to_string(minStaminaFloor) + ", isHorseHealthGold=" + std::to_string(isHorseHealthGold) + ", isHorseStaminaGold=" + std::to_string(isHorseStaminaGold));

    WriteLog(LogLevel::Standard, "Horse Core Sync Tick: Health=" + std::to_string(targetHealth) + ", Stamina=" + std::to_string(targetStamina));

    ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Health), (int)targetHealth);
    ATTRIBUTE::_SET_ATTRIBUTE_CORE_VALUE(horsePed, static_cast<int>(CoreIndex::Stamina), (int)targetStamina);
}

void UpdateCoreSimulation() {
    Ped playerPed = PLAYER::PLAYER_PED_ID();

    if (!ENTITY::DOES_ENTITY_EXIST(playerPed)) return;

    ProcessHotReload();

    CoreConfigSettings localCoreConfig;
	GeneralConfigSettings localGeneralConfig;
	PlayerConfigSettings localPlayerConfig;
	HorseConfigSettings localHorseConfig;
    {
        std::lock_guard<std::mutex> lock(g_ConfigMutex);
        localCoreConfig = g_CoreConfig;
		localGeneralConfig = g_GeneralConfig;
		localPlayerConfig = g_PlayerConfig;
        localHorseConfig = g_HorseConfig;
    }
    GameplayContext context = EvaluateGameplayState(playerPed, localGeneralConfig);
    int currentTotalMinutes = GetTotalInGameMinutes();

    if (HandleStateTransitions(playerPed, context, currentTotalMinutes, localPlayerConfig, localHorseConfig)) return;

    if (!context.freezeActive) {
        float hoursDelta = CalculateTimescaleDelta(currentTotalMinutes);
        
        if (hoursDelta <= 0.0f) return;

        ProcessPlayerSimulation(playerPed, hoursDelta, context, localGeneralConfig, localPlayerConfig);
        ProcessHorseSimulation(playerPed, hoursDelta, context, localPlayerConfig, localHorseConfig);
    }
}

void ScriptMain() {
    g_IniPath = ".\\CoreManagement.ini";
    g_LogPath = ".\\CoreManagement.log";

    // Wipe the log file on each script load to avoid excessive file growth
	std::ofstream(g_LogPath, std::ios::out | std::ios::trunc).close();

    StartAsyncLogger();

    LoadConfiguration();
    InitializeScenarioHashes();

    WriteLog(LogLevel::Standard, "==== Core Drain Management Simulation Engine Initialised Successfully [" + VERSION + "] ====");

	// Start the INI watcher thread to monitor for configuration changes
    std::thread watcher(IniWatcherThread);
    watcher.detach();

    while (true) {
        UpdateCoreSimulation();
        scriptWait(g_TickIntervalMs.load());
    }

    WriteLog(LogLevel::Standard, "==== Core Drain Management Simulation Engine Deinitialised Successfully [" + VERSION + "] ==== ====");

    StopAsyncLogger();
}