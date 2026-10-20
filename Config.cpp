/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <windows.h>
#include <iomanip>
#include <string>
#include <sstream>
#include <charconv>
#include <atomic>
#include <algorithm>
#include <fstream>
#include "Config.h"

#ifdef DEVTEST
const bool ALLOW_DEV_LOGGING = true;
#else
const bool ALLOW_DEV_LOGGING = false;
#endif

std::string VERSION = "v0.11-ALPHA";

std::mutex g_ConfigMutex;

CoreConfigSettings g_CoreConfig;
GeneralConfigSettings g_GeneralConfig;
PlayerConfigSettings g_PlayerConfig;
HorseConfigSettings g_HorseConfig;

CoreConfigSettings defaultCoreConfig;
GeneralConfigSettings defaultGeneralConfig;
PlayerConfigSettings defaultPlayerConfig;
HorseConfigSettings defaultHorseConfig;

std::atomic<int> g_CurrentLogLevel{ static_cast<int>(LogLevel::Disabled) };

std::atomic<bool> g_ShouldReloadConfig(false);

std::string g_IniPath = ".\\CoreManagement.ini";

std::atomic<int> g_stateChangeTickIntervalMs(500);

float GetIniFloat(const char* section, const char* key, float defaultValue, const char* filePath) {
    char defaultBuffer[32];
    auto [outPtr, outEc] = std::to_chars(defaultBuffer, defaultBuffer + sizeof(defaultBuffer) - 1, defaultValue);
    *outPtr = '\0'; // Fast, safe null-termination

    char resultBuffer[64];
    GetPrivateProfileStringA(section, key, defaultBuffer, resultBuffer, sizeof(resultBuffer), filePath);

    float parsedValue = defaultValue;
    std::string_view resultView(resultBuffer);

    auto [inPtr, inEc] = std::from_chars(resultView.data(), resultView.data() + resultView.size(), parsedValue);

    return parsedValue;
}

int GetIniInt(const char* section, const char* key, int defaultValue, const char* filePath) {
    return GetPrivateProfileIntA(section, key, defaultValue, filePath);
}

bool GetIniBool(const char* section, const char* key, bool defaultValue, const char* filePath) {
    int defaultInt = defaultValue ? 1 : 0;
    int result = GetPrivateProfileIntA(section, key, defaultInt, filePath);

    return result != 0;
}

bool DoesFileExist(const std::string& filePath) {
    DWORD fileAttributes = GetFileAttributesA(filePath.c_str());
    return (fileAttributes != INVALID_FILE_ATTRIBUTES && !(fileAttributes & FILE_ATTRIBUTE_DIRECTORY));
}

int ValidateLogLevel(int value, int defaultValue) {
    // 2105 is a hardcoded dev log level for very verbose debugging, not intended for gameplay (disabled in release builds)
    if (value == 0 || value == 1 || value == 2 || value == 2105) {
        if (!ALLOW_DEV_LOGGING && value == 2105)
            return 2; // If dev logging is not allowed, revert to Verbose

        return value;
    }

    return defaultValue;
}

void BuildDefaultConfigFile() {
    std::ofstream outFile(g_IniPath, std::ios::out | std::ios::trunc);
    if (outFile.is_open()) {
        outFile << "; CoreManagement.ini " << VERSION << "\n\n"
            << "[Performance]\n"
            << "; How often the main drain game simulation thread ticks (in milliseconds)\n"
            << "; Minimum interval is 5 seconds (5000 ms). Maximum is 5 mins (300000 ms)\n"
            << "coreDrainTickIntervalMs = " << defaultCoreConfig.coreDrainTickIntervalMs << "\n"
            << "; How often the main state simulation should run (in milliseconds). \n"
            << "; This should be low to capture mission triggers etc. The drain will be forced  in such cases\n"
            << "; Just leave it at 100 unless it causes performance issues\n"
            << "; Adjust log level first though as that is definitely the first place to gain performance\n"
			<< "; Minimum is 100 ms (100 ms). Maximum interval is 1 second (1000 ms).\n"
            << "stateChangeTickIntervalMs = " << defaultCoreConfig.stateChangeTickIntervalMs << "\n\n"

            << "[Logging]\n"
            << "; 0 = Disabled (Max performance), 1 = Standard, 2 = Verbose (Not recommended for normal play)\n"
            << "LogLevel = " << defaultCoreConfig.logLevelRaw << "\n\n"

            << "[General]\n"
            << "; in-game hour in which night officially begins\n"
            << "NightStartHour = " << defaultGeneralConfig.nightStartHour << "\n"
            << "; in-game hour in which day officially begins\n"
            << "NightEndHour = " << defaultGeneralConfig.nightEndHour << "\n"
            << "; If 1 (true), core drain ticks continuously during story/world missions. \n"
            << "EnableDuringMissions = " << static_cast<int>(defaultGeneralConfig.enableDuringMissions) << "\n"
            << "; If 1 (true), core drain ticks continuously while sitting at poker, blackjack, etc.\n"
            << "EnableDuringMinigames = " << static_cast<int>(defaultGeneralConfig.enableDuringMinigames) << "\n\n"

            << "[BaseDecayAwake]\n"
            << "; Points of core decay per in-game hour\n"
            << "HealthDecayBase = " << defaultPlayerConfig.baseHealthDecay << "\n"
            << "StaminaDecayBase = " << defaultPlayerConfig.baseStaminaDecay << "\n"
            << "DeadEyeDecayBase = " << defaultPlayerConfig.baseDeadEyeDecay << "\n\n"

            << "[Modifiers]\n"
            << "; percentage based modifiers for decay points above (0.5 = 50% drain [5 decay becomes 2.5 decay])\n"
            << "; HealthTimeSkipMultiplier - Decay rate multiplier for HEALTH when the player is asleep\n"
            << "; NightDeadEyeMultiplier   - Decay rate multiplier for the DEAD EYE during the night hours (NightStartHour -> NightEndHour)\n"
            << "; CampedMultiplier         - Decay rate multiplier for the all cores if the player is in a restrained state (e.g., camp/jail/bath/barber)\n"
            << ";                            In bathing scenarios this is just your HEALTH as bathing is rewarding for STAMINA and DEAD EYE\n"
            << "HealthTimeSkipMultiplier = " << defaultPlayerConfig.healthTimeSkipMultiplier << "\n"
            << "NightDeadEyeMultiplier = " << defaultPlayerConfig.nightDeadEyeMultiplier << "\n"
            << "CampedMultiplier = " << defaultPlayerConfig.campedMultiplier << "\n\n"

            << "[PlayerMovementModifiers]\n"
            << "; percentage based modifiers for decay points above (0.5 = 50% drain [5 decay becomes 2.5 decay]) depending movement speed of mount\n"
            << "JogStaminaMultiplier = " << defaultPlayerConfig.jogStaminaMultiplier << "\n"
            << "SprintStaminaMultiplier = " << defaultPlayerConfig.sprintStaminaMultiplier << "\n"
            << "MountedWalkStaminaMultiplier = " << defaultPlayerConfig.mountWalkStaminaMultiplier << "\n"
            << "MountedTrotStaminaMultiplier = " << defaultPlayerConfig.mountTrotStaminaMultiplier << "\n"
            << "MountedGallopStaminaMultiplier = " << defaultPlayerConfig.mountGallopStaminaMultiplier << "\n\n"

            << "[Floors]\n"
            << "; minimum core levels in the event of a time skip (fast-travel, minigames, sleep).\n"
            << "; is inactive for the corresponding gameplay type if EnableDuringMinigames is active. Always active for sleep and other events\n"
            << "; in general gameplay these are ignored and you can drop to zero core\n"
            << "; for example, when you sleep for 12 hours you will awaken with minimum cores of:\n"
			<< "; Health - 15.0\n" // todo: add stamina and dead eye values here too, as well as a non -sleep example for the player to understand the difference between sleep and non-sleep time skips
            << "HealthTimeSkipFloor = " << defaultPlayerConfig.healthTimeSkipFloor << "\n"
            << "CampedHealthFloor = " << defaultPlayerConfig.campedHealthFloor << "\n"
            << "CampedStaminaFloor = " << defaultPlayerConfig.campedStaminaFloor << "\n"
            << "CampedDeadEyeFloor = " << defaultPlayerConfig.campedDeadEyeFloor << "\n\n"

            << "[HorseBaseDecay]\n"
            << "; Points decay per in-game hour\n"
            << "HorseHealthDecayBase = " << defaultHorseConfig.baseHealthDecay << "\n"
            << "HorseStaminaDecayBase = " << defaultHorseConfig.baseStaminaDecay << "\n\n"

            << "[HorseMovementModifiers]\n"
            << "; percentage based modifiers for decay points above (0.5 = 50% drain [5 decay becomes 2.5 decay]) depending movement speed of mount\n"
            << "HorseGrazeStaminaMultiplier = " << defaultHorseConfig.grazeStaminaMultiplier << "\n"
            << "HorseTrotStaminaMultiplier = " << defaultHorseConfig.trotStaminaMultiplier << "\n"
            << "HorseGallopStaminaMultiplier = " << defaultHorseConfig.gallopStaminaMultiplier << "\n\n"

            << "[HorseFloors]\n"
            << "; minimum core levels in the event of a time skip (fast-travel, minigames, sleep).\n"
            << "; is inactive for the corresponding gameplay type if EnableDuringMinigames is active. Always active for sleep and other events\n"
            << "; in general gameplay these are ignored and you can drop to 0.0\n"
            << "HorseHealthTimeSkipFloor = " << defaultHorseConfig.healthTimeSkipFloor << "\n"
            << "CampedHorseHealthFloor = " << defaultHorseConfig.campedHealthFloor << "\n"
            << "CampedHorseStaminaFloor = " << defaultHorseConfig.campedStaminaFloor << "\n";
        outFile.close();
    }
}

void LogConfiguration() {  
    if (g_CurrentLogLevel.load(std::memory_order_acquire) < static_cast<int>(LogLevel::Standard))
        return;

    std::ostringstream table;
    table << std::fixed << std::setprecision(2)
        << "\n\n+---------------------------------------+------------+\n"
        << "| Configuration Parameter                   |      Value |\n"
        << "+-------------------------------------------+------------+\n"
        << "| Logging: Log Level                        | " << std::setw(10) << g_CoreConfig.logLevelRaw << " |\n"
        << "|                                           |            |\n"
        << "| Performance: Drain Tick Interval (ms)     | " << std::setw(10) << g_CoreConfig.coreDrainTickIntervalMs << " |\n"
        << "| Performance: State Tick Interval (ms)     | " << std::setw(10) << g_CoreConfig.stateChangeTickIntervalMs << " |\n"
        << "|                                           |            |\n"
        << "| General: Night Start Hour                 | " << std::setw(10) << g_GeneralConfig.nightStartHour << " |\n"
        << "| General: Night End Hour                   | " << std::setw(10) << g_GeneralConfig.nightEndHour << " |\n"
        << "| General: Drain Enabled for Missions       | " << std::setw(10) << static_cast<int>(g_GeneralConfig.enableDuringMissions) << " |\n"
        << "| General: Drain Enabled for Mini-games     | " << std::setw(10) << static_cast<int>(g_GeneralConfig.enableDuringMinigames) << " |\n"
        << "|                                           |            |\n"
        << "| Player: Health Base Awake Decay           | " << std::setw(10) << g_PlayerConfig.baseHealthDecay << " |\n"
        << "| Player: Stamina Base Awake Decay          | " << std::setw(10) << g_PlayerConfig.baseStaminaDecay << " |\n"
        << "| Player: Dead Eye Base Awake Decay         | " << std::setw(10) << g_PlayerConfig.baseDeadEyeDecay << " |\n"
        << "|                                           |            |\n"
        << "| Player: Sleep Health Multiplier           | " << std::setw(10) << g_PlayerConfig.healthTimeSkipMultiplier << " |\n"
        << "| Player: Night Dead Eye Multiplier         | " << std::setw(10) << g_PlayerConfig.nightDeadEyeMultiplier << " |\n"
        << "| Player: Camped/Restrained Multiplier      | " << std::setw(10) << g_PlayerConfig.campedMultiplier << " |\n"
        << "|                                           |            |\n"
        << "| Player: Jog Stamina Multiplier            | " << std::setw(10) << g_PlayerConfig.jogStaminaMultiplier << " |\n"
        << "| Player: Sprint Stamina Multiplier         | " << std::setw(10) << g_PlayerConfig.sprintStaminaMultiplier << " |\n"
        << "|                                           |            |\n"
		<< "| Player: Mounted Walk Stamina Multiplier   | " << std::setw(10) << g_PlayerConfig.mountWalkStaminaMultiplier << " |\n"
        << "| Player: Mounted Trot Stamina Multiplier   | " << std::setw(10) << g_PlayerConfig.mountTrotStaminaMultiplier << " |\n"
        << "| Player: Mounted Gallop Stamina Multiplier | " << std::setw(10) << g_PlayerConfig.mountGallopStaminaMultiplier << " |\n"
        << "|                                           |            |\n"
        << "| Player: Health Timeskip Floor             | " << std::setw(10) << g_PlayerConfig.healthTimeSkipFloor << " |\n"
        << "| Player: Health Camped Floor               | " << std::setw(10) << g_PlayerConfig.campedHealthFloor << " |\n"
        << "| Player: Stamina Camped Floor              | " << std::setw(10) << g_PlayerConfig.campedStaminaFloor << " |\n"
        << "| Player: Dead Eye Camped Floor             | " << std::setw(10) << g_PlayerConfig.campedDeadEyeFloor << " |\n"
        << "|                                           |            |\n"
        << "| Horse: Health Base Decay                  | " << std::setw(10) << g_HorseConfig.baseHealthDecay << " |\n"
        << "| Horse: Stamina Base Decay                 | " << std::setw(10) << g_HorseConfig.baseStaminaDecay << " |\n"
        << "|                                           |            |\n"
		<< "| Horse: Stamina Graze Multiplier           | " << std::setw(10) << g_HorseConfig.grazeStaminaMultiplier << " |\n"
        << "| Horse: Stamina Trot Multiplier            | " << std::setw(10) << g_HorseConfig.trotStaminaMultiplier << " |\n"
        << "| Horse: Stamina Gallop Multiplier          | " << std::setw(10) << g_HorseConfig.gallopStaminaMultiplier << " |\n"
        << "|                                           |            |\n"
        << "| Horse: Health Timeskip Floor              | " << std::setw(10) << g_HorseConfig.healthTimeSkipFloor << " |\n"
        << "| Horse: Health Camped Floor                | " << std::setw(10) << g_HorseConfig.campedHealthFloor << " |\n"
        << "| Horse: Stamina Camped Floor               | " << std::setw(10) << g_HorseConfig.campedStaminaFloor << " |\n"
        << "+-------------------------------------------+------------+\n";

    WriteLog(LogLevel::Standard, "Internal Parameter Map Hydrated:" + table.str());
}

void LoadConfiguration() {

    if (!DoesFileExist(g_IniPath)) {
        BuildDefaultConfigFile();

        WriteLog(LogLevel::Standard, "Configuration file missing. Generating fresh defaults at: " + g_IniPath);
    }

    WriteLog(LogLevel::Standard, "Loading ini configuration from: " + g_IniPath);

    g_CoreConfig.coreDrainTickIntervalMs = std::clamp(GetIniInt("Performance", "coreDrainTickIntervalMs", defaultCoreConfig.coreDrainTickIntervalMs, g_IniPath.c_str()), 5000, 300000); // minimum 5 second tick interval maximum 5 minute tick interval
		                                                                                                                                                                                // it's designed to be a slow loop as core drain 
                                                                                                                                                                                        // isn't really a real-time process        
        
	g_CoreConfig.stateChangeTickIntervalMs = std::clamp(GetIniInt("Performance", "stateChangeTickIntervalMs", defaultCoreConfig.stateChangeTickIntervalMs, g_IniPath.c_str()), 100, 1000); // clamp between 0.1 and 1 second tick interval 
	                                                                                                                                                                                       // it's designed to be a fast loop (very lightweight)
                                                                                                                                                                                           // to avoid excessive CPU usage while not too slow to miss state changes

	g_CoreConfig.logLevelRaw = ValidateLogLevel(GetIniInt("Logging", "LogLevel", defaultCoreConfig.logLevelRaw, g_IniPath.c_str()), defaultCoreConfig.logLevelRaw); // minimum log level is 0 (disabled) max is a hardcoded 2105 (dev- very verbose not for gameplay, will effect fps). 
                                                                                                                                        // Range is 0-2 for normal gameplay. 0=disabled, 1=standard, 2=verbose
    g_GeneralConfig.nightStartHour = std::clamp(GetIniInt("General", "NightStartHour", defaultGeneralConfig.nightStartHour, g_IniPath.c_str()), 0, 23); // clamp between 0 and 23
    g_GeneralConfig.nightEndHour = std::clamp(GetIniInt("General", "NightEndHour", defaultGeneralConfig.nightEndHour, g_IniPath.c_str()), 0, 23); // clamp between 0 and 23
    g_GeneralConfig.enableDuringMissions = GetIniBool("General", "EnableDuringMissions", defaultGeneralConfig.enableDuringMissions, g_IniPath.c_str());
    g_GeneralConfig.enableDuringMinigames = GetIniBool("General", "EnableDuringMinigames", defaultGeneralConfig.enableDuringMinigames, g_IniPath.c_str());

    g_PlayerConfig.baseHealthDecay = std::clamp(GetIniFloat("BaseDecayAwake", "HealthDecayBase", defaultPlayerConfig.baseHealthDecay, g_IniPath.c_str()), 1.0f, 100.0f);
    g_PlayerConfig.baseStaminaDecay = std::clamp(GetIniFloat("BaseDecayAwake", "StaminaDecayBase", defaultPlayerConfig.baseStaminaDecay, g_IniPath.c_str()), 1.0f, 100.0f);
    g_PlayerConfig.baseDeadEyeDecay = std::clamp(GetIniFloat("BaseDecayAwake", "DeadEyeDecayBase", defaultPlayerConfig.baseDeadEyeDecay, g_IniPath.c_str()), 1.0f, 100.0f);

    g_PlayerConfig.healthTimeSkipMultiplier = std::clamp(GetIniFloat("Modifiers", "HealthTimeSkipMultiplier", defaultPlayerConfig.healthTimeSkipMultiplier, g_IniPath.c_str()), 0.01f, 10.0f);
    g_PlayerConfig.nightDeadEyeMultiplier = std::clamp(GetIniFloat("Modifiers", "NightDeadEyeMultiplier", defaultPlayerConfig.nightDeadEyeMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);
    g_PlayerConfig.campedMultiplier = std::clamp(GetIniFloat("Modifiers", "CampedMultiplier", defaultPlayerConfig.campedMultiplier, g_IniPath.c_str()), 0.01f, 10.0f);

    g_PlayerConfig.jogStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "JogStaminaMultiplier", defaultPlayerConfig.jogStaminaMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);
    g_PlayerConfig.sprintStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "SprintStaminaMultiplier", defaultPlayerConfig.sprintStaminaMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);

	g_PlayerConfig.mountWalkStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "MountedWalkStaminaMultiplier", defaultPlayerConfig.mountWalkStaminaMultiplier, g_IniPath.c_str()), 0.5f, 10.0f);
    g_PlayerConfig.mountTrotStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "MountedTrotStaminaMultiplier", defaultPlayerConfig.mountTrotStaminaMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);
    g_PlayerConfig.mountGallopStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "MountedGallopStaminaMultiplier", defaultPlayerConfig.mountGallopStaminaMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);

    g_PlayerConfig.healthTimeSkipFloor = std::clamp(GetIniFloat("Floors", "HealthTimeSkipFloor", defaultPlayerConfig.healthTimeSkipFloor, g_IniPath.c_str()), 1.0f, 15.0f);
    g_PlayerConfig.campedHealthFloor = std::clamp(GetIniFloat("Floors", "CampedHealthFloor", defaultPlayerConfig.campedHealthFloor, g_IniPath.c_str()), 5.0f, 10.0f);
    g_PlayerConfig.campedStaminaFloor = std::clamp(GetIniFloat("Floors", "CampedStaminaFloor", defaultPlayerConfig.campedStaminaFloor, g_IniPath.c_str()), 1.0f, 10.0f);
    g_PlayerConfig.campedDeadEyeFloor = std::clamp(GetIniFloat("Floors", "CampedDeadEyeFloor", defaultPlayerConfig.campedDeadEyeFloor, g_IniPath.c_str()), 1.0f, 10.0f);

    g_HorseConfig.baseHealthDecay = std::clamp(GetIniFloat("HorseBaseDecay", "HorseHealthDecayBase", defaultHorseConfig.baseHealthDecay, g_IniPath.c_str()), 1.0f, 100.0f);
    g_HorseConfig.baseStaminaDecay = std::clamp(GetIniFloat("HorseBaseDecay", "HorseStaminaDecayBase", defaultHorseConfig.baseStaminaDecay, g_IniPath.c_str()), 1.0f, 100.0f);

	g_HorseConfig.grazeStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseGrazeStaminaMultiplier", defaultHorseConfig.grazeStaminaMultiplier, g_IniPath.c_str()), 0.5f, 10.0f);
    g_HorseConfig.trotStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseTrotStaminaMultiplier", defaultHorseConfig.trotStaminaMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);
    g_HorseConfig.gallopStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseGallopStaminaMultiplier", defaultHorseConfig.gallopStaminaMultiplier, g_IniPath.c_str()), 1.0f, 10.0f);

    g_HorseConfig.healthTimeSkipFloor = std::clamp(GetIniFloat("HorseFloors", "HorseHealthTimeSkipFloor", defaultHorseConfig.healthTimeSkipFloor, g_IniPath.c_str()), 1.0f, 15.0f);
    g_HorseConfig.campedHealthFloor = std::clamp(GetIniFloat("HorseFloors", "CampedHealthFloor", defaultHorseConfig.campedHealthFloor, g_IniPath.c_str()), 1.0f, 10.0f);
    g_HorseConfig.campedStaminaFloor = std::clamp(GetIniFloat("HorseFloors", "CampedStaminaFloor", defaultHorseConfig.campedStaminaFloor, g_IniPath.c_str()), 1.0f, 10.0f);

	switch (g_CoreConfig.logLevelRaw) {
	case 0:
		g_CoreConfig.currentLogLevel = LogLevel::Disabled;
		break;
	case 1:
		g_CoreConfig.currentLogLevel = LogLevel::Standard;
		break;
	case 2:
		g_CoreConfig.currentLogLevel = LogLevel::Verbose;
		break;
	case 2105:
		g_CoreConfig.currentLogLevel = LogLevel::Dev;
		break;
	default:
		g_CoreConfig.currentLogLevel = LogLevel::Disabled; // Fallback to disabled if somehow an invalid value is present
		break;
	}

    g_stateChangeTickIntervalMs.store(g_CoreConfig.stateChangeTickIntervalMs);
    
    g_CurrentLogLevel.store(static_cast<int>(g_CoreConfig.logLevelRaw), std::memory_order_release);

    LogConfiguration();
}