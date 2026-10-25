/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <memory>
#include <utility>
#include "Logger.h"

extern const std::string VERSION;

struct CoreConfigSettings {
    int coreDrainTickIntervalMs = 10000;
    int stateChangeTickIntervalMs = 500;
    float restrictedStateChangeTickIntervalMultiplier = 1.0f;
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
	float deadEyeDirtyMultiplier = 1.1f;
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

struct ConfigSnapshot {
    std::shared_ptr<CoreConfigSettings>    corePtr;
    std::shared_ptr<GeneralConfigSettings> generalPtr;
    std::shared_ptr<PlayerConfigSettings>  playerPtr;
    std::shared_ptr<HorseConfigSettings>   horsePtr;

    // Expose clean direct references using dot notation
    const CoreConfigSettings& CoreConfig;
    const GeneralConfigSettings& GeneralConfig;
    const PlayerConfigSettings& PlayerConfig;
    const HorseConfigSettings& HorseConfig;

    ConfigSnapshot(
        std::shared_ptr<CoreConfigSettings>    core,
        std::shared_ptr<GeneralConfigSettings> general,
        std::shared_ptr<PlayerConfigSettings>  player,
        std::shared_ptr<HorseConfigSettings>   horse)
        : corePtr(std::move(core)), generalPtr(std::move(general)),
        playerPtr(std::move(player)), horsePtr(std::move(horse)),
        CoreConfig(*corePtr), GeneralConfig(*generalPtr),
        PlayerConfig(*playerPtr), HorseConfig(*horsePtr)
    {}
};

extern std::atomic<bool> g_HasConfigChanged;

extern const std::string g_IniPath;

extern std::atomic<int> g_StateChangeTickIntervalMs;
extern std::atomic<int> g_StateChangeTickIntervalRestrictedMs;
extern std::atomic<int> g_GameTimescaleMS;

extern std::atomic<std::shared_ptr<CoreConfigSettings>> g_CoreConfig;
extern std::atomic<std::shared_ptr<GeneralConfigSettings>> g_GeneralConfig;
extern std::atomic<std::shared_ptr<PlayerConfigSettings>> g_PlayerConfig;
extern std::atomic<std::shared_ptr<HorseConfigSettings>> g_HorseConfig;

void ConfigWatcherWorker();

void LoadConfiguration();

extern std::atomic<bool> g_RunConfigThread;
extern std::condition_variable g_ConfigThreadCV;
extern std::mutex g_ConfigMutex;