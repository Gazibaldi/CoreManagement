/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <mutex>
#include <atomic>
#include "Logger.h"

extern std::string VERSION;

// Explicit extern linkage to look up your main configuration settings
struct CoreConfigSettings {
    int coreDrainTickIntervalMs = 5000;
    int stateChangeTickIntervalMs = 500;
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
    float nightDeadEyeMultiplier = 1.3f;
    float campJailBathMultiplier = 0.25f;
    float healthTimeSkipFloor = 15.0f;
    float restrainedHealthFloor = 5.0f;
    float restrainedStaminaFloor = 5.0f;
    float restrainedDeadEyeFloor = 5.0f;
};

struct HorseConfigSettings {
    float baseHealthDecay = 3.5f;
    float baseStaminaDecay = 5.5f;
    float trotStaminaMultiplier = 1.3f;
    float gallopStaminaMultiplier = 4.5f;
    float restrainedHealthFloor = 5.0f;
    float restrainedStaminaFloor = 5.0f;
};

extern std::mutex g_ConfigMutex;
extern std::atomic<int> g_CurrentLogLevel;
extern std::atomic<bool> g_RunWatcherThread;
extern std::condition_variable g_WatcherCv;
extern std::atomic<bool> g_ShouldReloadConfig;
extern std::string g_IniPath;
extern std::atomic<int> g_coreDrainTickIntervalMs;
extern std::atomic<int> g_stateChangeTickIntervalMs;

extern CoreConfigSettings g_CoreConfig;
extern GeneralConfigSettings g_GeneralConfig;
extern PlayerConfigSettings g_PlayerConfig;
extern HorseConfigSettings g_HorseConfig;

void IniWatcherThread();
void LoadConfiguration();