#pragma once
#include "Logger.h"
#include <mutex>
#include <atomic>

#ifndef MAX
#define MAX(value, floorLimit) ((value) > (floorLimit) ? (value) : (floorLimit))
#endif // !MAX

#ifndef MIN
#define MIN(value, ceilingLimit) ((value) < (ceilingLimit) ? (value) : (ceilingLimit))
#endif // !MIN

// Explicit extern linkage to look up your main configuration settings
struct CoreConfigSettings {
    int mainLoopTickIntervalMs = 1000;
    int logLevelRaw = 1;

    LogLevel currentLogLevel = LogLevel::Standard;
};

struct GeneralConfigSettings {
    int nightStartHour = 19;
    int nightEndHour = 7;

    bool allowDrainInMissions = false;
    bool allowDrainInMinigames = false;
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

extern std::mutex g_ConfigMutex;
extern std::atomic<int> g_CurrentLogLevel;
extern std::atomic<bool> g_RunWatcherThread;
extern std::atomic<bool> g_ShouldReloadConfig;
extern std::string g_IniPath;
extern std::atomic<int> g_TickIntervalMs;

extern CoreConfigSettings g_CoreConfig;
extern GeneralConfigSettings g_GeneralConfig;
extern PlayerConfigSettings g_PlayerConfig;
extern HorseConfigSettings g_HorseConfig;

void IniWatcherThread();
void LoadConfiguration();