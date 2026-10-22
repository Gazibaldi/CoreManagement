/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <mutex>
#include <atomic>
#include <string>
#include "Logger.h"

extern std::string VERSION;

struct CoreConfigSettings {
    int coreDrainTickIntervalMs = 10000;
    int stateChangeTickIntervalMs = 500;
    int logLevelRaw = 1;

    LogLevel currentLogLevel = LogLevel::Standard;
};

struct GeneralConfigSettings {
    int msPerInGameMinute = 2000;
    int nightStartHour = 20;
    int nightEndHour = 6;

    bool enableDuringMissions = false;
    bool enableDuringMinigames = false;
};

struct PlayerConfigSettings {
    float baseHealthDecay = 3.5f;
    float baseStaminaDecay = 5.5f;
    float baseDeadEyeDecay = 4.16f;

    float healthTimeSkipMultiplier = 0.5f;
    float nightDeadEyeMultiplier = 1.3f;
    float campedMultiplier = 0.25f;

    float jogStaminaMultiplier = 1.2f;
    float sprintStaminaMultiplier = 1.5f;


    float mountWalkStaminaMultiplier = 0.8f;
    float mountTrotStaminaMultiplier = 1.0f;
    float mountGallopStaminaMultiplier = 1.2f;

    float healthTimeSkipFloor = 15.0f;
    float campedHealthFloor = 5.0f;
    float campedStaminaFloor = 5.0f;
    float campedDeadEyeFloor = 5.0f;
};

struct HorseConfigSettings {
    float baseHealthDecay = 3.5f;
    float baseStaminaDecay = 5.5f;

    float grazeStaminaMultiplier = 0.7f;
    float trotStaminaMultiplier = 1.3f;
    float gallopStaminaMultiplier = 4.5f;

    float healthTimeSkipFloor = 15.0f;
    float campedHealthFloor = 5.0f;
    float campedStaminaFloor = 5.0f;
};

extern std::mutex g_ConfigMutex;

extern std::atomic<int> g_CurrentLogLevel;
extern std::atomic<bool> g_ShouldReloadConfig;

extern std::string g_IniPath;

extern std::atomic<int> g_StateChangeTickIntervalMs;
extern std::atomic<int> g_GameTimescaleMS;

extern CoreConfigSettings g_CoreConfig;
extern GeneralConfigSettings g_GeneralConfig;
extern PlayerConfigSettings g_PlayerConfig;
extern HorseConfigSettings g_HorseConfig;

void LoadConfiguration();