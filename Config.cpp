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
#include <string_view>
#include <memory>
#include <mutex>
#include <condition_variable>
#include "Config.h"
#include "Logger.h"

#ifdef DEVTEST
const bool ALLOW_DEV_LOGGING = true;
#else
const bool ALLOW_DEV_LOGGING = false;
#endif

const std::string VERSION = "v0.12-ALPHA";

std::atomic<std::shared_ptr<CoreConfigSettings>> g_CoreConfig{ std::make_shared<CoreConfigSettings>() };
std::atomic<std::shared_ptr<GeneralConfigSettings>> g_GeneralConfig{ std::make_shared<GeneralConfigSettings>() };
std::atomic<std::shared_ptr<PlayerConfigSettings>> g_PlayerConfig{ std::make_shared<PlayerConfigSettings>() };
std::atomic<std::shared_ptr<HorseConfigSettings>> g_HorseConfig{ std::make_shared<HorseConfigSettings>() };

const CoreConfigSettings defaultCoreConfig;
const GeneralConfigSettings defaultGeneralConfig;
const PlayerConfigSettings defaultPlayerConfig;
const HorseConfigSettings defaultHorseConfig;

const std::string g_IniPath = ".\\CoreManagement.ini";

std::atomic<int> g_StateChangeTickIntervalMs(500);
std::atomic<int> g_StateChangeTickIntervalRestrictedMs(500);

std::atomic<bool> g_RunConfigThread(false);
std::condition_variable g_ConfigThreadCV;

std::mutex g_ConfigMutex;
std::atomic<bool> g_HasConfigChanged(false);
std::atomic<bool> g_ShouldHotReload(true);

void EnableConfigHotReload(bool enableHotReload) {
	bool currentState = g_ShouldHotReload.load(std::memory_order_acquire);

	if (currentState != enableHotReload) {
        LOG(LogLevel::ConfigOnly, "Config hot-reload is disabled due to player being in a mission or minigame. Config will be refreshed once the player exits these states.");
        g_ShouldHotReload.store(enableHotReload, std::memory_order_release);
	}
}

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

void ConfigWatcherWorker() {
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    FILETIME lastWriteTime = { 0, 0 };

    if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
        lastWriteTime = fileData.ftLastWriteTime;
    }

    while (g_RunConfigThread) {
        {
            std::unique_lock<std::mutex> lock(g_ConfigMutex);

            g_ConfigThreadCV.wait_for(lock, std::chrono::milliseconds(8000), [&] {
                return !g_RunConfigThread.load(std::memory_order_acquire);
                });
        }

        // Check again immediately after waking up to handle instant shutdown
        if (!g_RunConfigThread) break;

        if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
            if (CompareFileTime(&fileData.ftLastWriteTime, &lastWriteTime) > 0 && g_ShouldHotReload.load(std::memory_order_acquire)) {
                lastWriteTime = fileData.ftLastWriteTime;

                // Give the OS text editor 100ms to finish flushing its buffer to disk cleanly
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                LOG(LogLevel::ConfigOnly, "Config change detected. Parsing INI...");

                LoadConfiguration(); // Overwrites g_CoreConfig, g_GeneralConfig, etc.
                
                g_HasConfigChanged.store(true, std::memory_order_release);
                LOG(LogLevel::ConfigOnly, "Configuration dynamically reloaded.");
            }
        }
    }
}

[[nodiscard]] std::thread InitialiseConfigThread() {
    g_RunConfigThread.store(true, std::memory_order_release);
    return std::thread(ConfigWatcherWorker);
}

void ShutdownConfig() {
    g_RunConfigThread.store(false, std::memory_order_release);

    // Wake up the condition variables loop in case it's waiting/sleeping
    {
        std::lock_guard<std::mutex> lock(g_ConfigMutex);
        g_ConfigThreadCV.notify_all();
    }
}

void BuildDefaultConfigFile() {
    std::ofstream outFile(g_IniPath, std::ios::out | std::ios::trunc);
    if (outFile.is_open()) {
        outFile << "; CoreManagement.ini " << VERSION << "\n\n"
            << "[Performance]\n"
            << "; How often the main drain game simulation thread ticks (in milliseconds)\n"
            << "; Minimum interval is 5 seconds (5000 ms). Maximum is 5 mins (300000 ms)\n"
            << "; Recommendation: keep this as a multiple of the msPerInGameMinute to ensure clean hour based decay delta\n"
            << "; Personal recommendation : set to 30 mins in game (vanilla: 2000ms per in game minute = 2000 * 30 = 60000)\n"
            << "; If you set the msPerInGameMinute close to real-time, reduce accordingly (15 mins in - game or even 10 mins)\n"
            << "coreDrainTickIntervalMs = " << defaultCoreConfig.coreDrainTickIntervalMs << "\n"
            << "; How often the main state simulation should run (in milliseconds)\n"
            << "; This should be low to capture mission triggers etc. The drain will be forced  in such cases\n"
            << "; Just leave it at 500 unless it causes performance issues\n"
            << "; Adjust log level first though as that is definitely the first place to gain performance\n"
			<< "; Minimum is 0.1 seconds (100 ms). Maximum interval is 1 second (1000 ms)\n"
            << "stateChangeTickIntervalMs = " << defaultCoreConfig.stateChangeTickIntervalMs << "\n"
            << "; This multiplier is applied to the state change tick interval when the player is in mission or minigame state\n"
            << "; The higher the value, the more the state change tick interval is increased, resulting in slower state changes but higher performance\n"
            << "; Only active if EnableDuringMissions or EnableDuringMinigames is false\n"
			<< "; Minimum is 1.0 (no change). Maximum is 3.0 (3x slower)\n"
            << "restrictedStateChangeTickIntervalMultiplier = " << defaultCoreConfig.restrictedStateChangeTickIntervalMultiplier << "\n\n"

            << "[Logging]\n"
            << "; 0 = Disabled (Max performance), 1 = Standard, 2 = Verbose (Not recommended for normal play)\n"
            << "LogLevel = " << defaultCoreConfig.logLevelRaw << "\n\n"

            << "[General]\n"
            << "; length of in-game minute in real-time milliseconds (vanilla: 2000ms - 2 seconds)\n"
            << "; Minimum interval is 2 seconds (2000 ms). Maximum is 60 seconds (60000 ms)\n"
           	<< "msPerInGameMinute = " << defaultGeneralConfig.msPerInGameMinute << "\n"
            << "; in-game hour in which night officially begins\n"
            << "; valid range : 18 - 22 (6pm - 10pm)\n"
            << "NightStartHour = " << defaultGeneralConfig.nightStartHour << "\n"
            << "; in-game hour in which day officially begins\n"
            << "; valid range 4 - 8 (4am - 8am)"
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
            << "; DeadEyeDirtyMultiplier   - Decay rate multiplier for the DEAD EYE when the player is dirty (1.0 = no change - 1.5 = 50% increase)\n"
            << "; CampedMultiplier         - Decay rate multiplier for the all cores if the player is in a restrained state (e.g., camp/jail/bath/barber)\n"
            << ";                            In bathing scenarios this is just your HEALTH as bathing is rewarding for STAMINA and DEAD EYE\n"
            << "HealthTimeSkipMultiplier = " << defaultPlayerConfig.healthTimeSkipMultiplier << "\n"
            << "NightDeadEyeMultiplier = " << defaultPlayerConfig.nightDeadEyeMultiplier << "\n"
            << "DeadEyeDirtyMultiplier = " << defaultPlayerConfig.deadEyeDirtyMultiplier << "\n"
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

void LogConfiguration(const CoreConfigSettings* coreConfig, const GeneralConfigSettings* generalConfig, const PlayerConfigSettings* playerConfig, const HorseConfigSettings* horseConfig) {
    std::ostringstream table;
    table << std::fixed << std::setprecision(2)
        << "+---------------------------------------------+------------+\n"
        << "| Configuration Parameter                     |      Value |\n"
        << "+---------------------------------------------+------------+\n"
        << "| Logging: Log Level                          | " << std::setw(10) << coreConfig->logLevelRaw << " |\n"
        << "|                                             |            |\n"
        << "| Performance: Drain Tick Interval (ms)       | " << std::setw(10) << coreConfig->coreDrainTickIntervalMs << " |\n"
        << "| Performance: State Tick Interval (ms)       | " << std::setw(10) << coreConfig->stateChangeTickIntervalMs << " |\n"
        << "| Performance: State Tick Interval Multiplier | " << std::setw(10) << coreConfig->restrictedStateChangeTickIntervalMultiplier << " |\n"
        << "|                                             |            |\n"
        << "| General: In-Game Minute (ms)                | " << std::setw(10) << generalConfig->msPerInGameMinute << " |\n"
        << "| General: Night Start Hour                   | " << std::setw(10) << generalConfig->nightStartHour << " |\n"
        << "| General: Night End Hour                     | " << std::setw(10) << generalConfig->nightEndHour << " |\n"
        << "| General: Drain Enabled for Missions         | " << std::setw(10) << static_cast<int>(generalConfig->enableDuringMissions) << " |\n"
        << "| General: Drain Enabled for Mini-games       | " << std::setw(10) << static_cast<int>(generalConfig->enableDuringMinigames) << " |\n"
        << "|                                             |            |\n"
        << "| Player: Health Base Awake Decay             | " << std::setw(10) << playerConfig->baseHealthDecay << " |\n"
        << "| Player: Stamina Base Awake Decay            | " << std::setw(10) << playerConfig->baseStaminaDecay << " |\n"
        << "| Player: Dead Eye Base Awake Decay           | " << std::setw(10) << playerConfig->baseDeadEyeDecay << " |\n"
        << "|                                             |            |\n"
        << "| Player: Sleep Health Multiplier             | " << std::setw(10) << playerConfig->healthTimeSkipMultiplier << " |\n"
        << "| Player: Night Dead Eye Multiplier           | " << std::setw(10) << playerConfig->nightDeadEyeMultiplier << " |\n"
        << "| Player: Dead Eye Dirty Multiplier           | " << std::setw(10) << playerConfig->deadEyeDirtyMultiplier << " |\n"
        << "| Player: Camped/Restrained Multiplier        | " << std::setw(10) << playerConfig->campedMultiplier << " |\n"
        << "|                                             |            |\n"
        << "| Player: Jog Stamina Multiplier              | " << std::setw(10) << playerConfig->jogStaminaMultiplier << " |\n"
        << "| Player: Sprint Stamina Multiplier           | " << std::setw(10) << playerConfig->sprintStaminaMultiplier << " |\n"
        << "|                                             |            |\n"
		<< "| Player: Mounted Walk Stamina Multiplier     | " << std::setw(10) << playerConfig->mountWalkStaminaMultiplier << " |\n"
        << "| Player: Mounted Trot Stamina Multiplier     | " << std::setw(10) << playerConfig->mountTrotStaminaMultiplier << " |\n"
        << "| Player: Mounted Gallop Stamina Multiplier   | " << std::setw(10) << playerConfig->mountGallopStaminaMultiplier << " |\n"
        << "|                                             |            |\n"
        << "| Player: Health Timeskip Floor               | " << std::setw(10) << playerConfig->healthTimeSkipFloor << " |\n"
        << "| Player: Health Camped Floor                 | " << std::setw(10) << playerConfig->campedHealthFloor << " |\n"
        << "| Player: Stamina Camped Floor                | " << std::setw(10) << playerConfig->campedStaminaFloor << " |\n"
        << "| Player: Dead Eye Camped Floor               | " << std::setw(10) << playerConfig->campedDeadEyeFloor << " |\n"
        << "|                                             |            |\n"
        << "| Horse: Health Base Decay                    | " << std::setw(10) << horseConfig->baseHealthDecay << " |\n"
        << "| Horse: Stamina Base Decay                   | " << std::setw(10) << horseConfig->baseStaminaDecay << " |\n"
        << "|                                             |            |\n"
		<< "| Horse: Stamina Graze Multiplier             | " << std::setw(10) << horseConfig->grazeStaminaMultiplier << " |\n"
        << "| Horse: Stamina Trot Multiplier              | " << std::setw(10) << horseConfig->trotStaminaMultiplier << " |\n"
        << "| Horse: Stamina Gallop Multiplier            | " << std::setw(10) << horseConfig->gallopStaminaMultiplier << " |\n"
        << "|                                             |            |\n"
        << "| Horse: Health Timeskip Floor                | " << std::setw(10) << horseConfig->healthTimeSkipFloor << " |\n"
        << "| Horse: Health Camped Floor                  | " << std::setw(10) << horseConfig->campedHealthFloor << " |\n"
        << "| Horse: Stamina Camped Floor                 | " << std::setw(10) << horseConfig->campedStaminaFloor << " |\n"
        << "+---------------------------------------------+------------+\n";

    LOG(LogLevel::ConfigOnly, "Internal Parameter Map Hydrated:\n{}", table.str());
}

void LoadConfiguration() {
    if (!DoesFileExist(g_IniPath)) {
        BuildDefaultConfigFile();

		LOG(LogLevel::ConfigOnly, "Configuration file missing. Generating fresh defaults at: {}", g_IniPath);
    }

    LOG(LogLevel::ConfigOnly, "Loading ini configuration from: {}", g_IniPath);

    std::shared_ptr<CoreConfigSettings>    newCoreConfig = std::make_shared<CoreConfigSettings>();
    std::shared_ptr<GeneralConfigSettings> newGeneralConfig = std::make_shared<GeneralConfigSettings>();
    std::shared_ptr<PlayerConfigSettings>  newPlayerConfig = std::make_shared<PlayerConfigSettings>();
	std::shared_ptr<HorseConfigSettings>   newHorseConfig = std::make_shared<HorseConfigSettings>();

    newCoreConfig->coreDrainTickIntervalMs = std::clamp(GetIniInt("Performance", "coreDrainTickIntervalMs", defaultCoreConfig.coreDrainTickIntervalMs, g_IniPath.c_str()), 5000, 300000); // minimum 5 second tick interval maximum 5 minute tick interval
		                                                                                                                                                                                // it's designed to be a slow loop as core drain 
                                                                                                                                                                                        // isn't really a real-time process        
        
	newCoreConfig->stateChangeTickIntervalMs = std::clamp(GetIniInt("Performance", "stateChangeTickIntervalMs", defaultCoreConfig.stateChangeTickIntervalMs, g_IniPath.c_str()), 100, 1000); // clamp between 0.1 and 1 second tick interval 
	                                                                                                                                                                                       // it's designed to be a fast loop (very lightweight)
                                                                                                                                                                                           // to avoid excessive CPU usage while not too slow to miss state changes
	newCoreConfig->restrictedStateChangeTickIntervalMultiplier = std::clamp(GetIniFloat("Performance", "restrictedStateChangeTickIntervalMultiplier", 
                                                                   defaultCoreConfig.restrictedStateChangeTickIntervalMultiplier, g_IniPath.c_str()), 1.0f, 3.0f); // clamp between 1.0 and 3.0 multiplier

	newCoreConfig->logLevelRaw = ValidateLogLevel(GetIniInt("Logging", "LogLevel", defaultCoreConfig.logLevelRaw, g_IniPath.c_str()), defaultCoreConfig.logLevelRaw); // minimum log level is 0 (disabled) max is a hardcoded 2105 (dev- very verbose not for gameplay, will effect fps). 
                                                                                                                                                                    // Range is 0-2 for normal gameplay. 0=disabled, 1=standard, 2=verbose

    newGeneralConfig->msPerInGameMinute = std::clamp(GetIniInt("General", "MsPerInGameMinute", defaultGeneralConfig.msPerInGameMinute, g_IniPath.c_str()), 2000, 60000); // clamp between 2000ms (2 seconds) and 60000ms (60 seconds)
    newGeneralConfig->nightStartHour = std::clamp(GetIniInt("General", "NightStartHour", defaultGeneralConfig.nightStartHour, g_IniPath.c_str()), 18, 22); // clamp between 18 and 22 (6pm - 10pm)
    newGeneralConfig->nightEndHour = std::clamp(GetIniInt("General", "NightEndHour", defaultGeneralConfig.nightEndHour, g_IniPath.c_str()), 4, 8); // clamp between 4 and 8 (4am - 8am)
    newGeneralConfig->enableDuringMissions = GetIniBool("General", "EnableDuringMissions", defaultGeneralConfig.enableDuringMissions, g_IniPath.c_str());
    newGeneralConfig->enableDuringMinigames = GetIniBool("General", "EnableDuringMinigames", defaultGeneralConfig.enableDuringMinigames, g_IniPath.c_str());

    newPlayerConfig->baseHealthDecay = std::clamp(GetIniFloat("BaseDecayAwake", "HealthDecayBase", defaultPlayerConfig.baseHealthDecay, g_IniPath.c_str()), 1.0f, 100.0f);
    newPlayerConfig->baseStaminaDecay = std::clamp(GetIniFloat("BaseDecayAwake", "StaminaDecayBase", defaultPlayerConfig.baseStaminaDecay, g_IniPath.c_str()), 1.0f, 100.0f);
    newPlayerConfig->baseDeadEyeDecay = std::clamp(GetIniFloat("BaseDecayAwake", "DeadEyeDecayBase", defaultPlayerConfig.baseDeadEyeDecay, g_IniPath.c_str()), 1.0f, 100.0f);

    newPlayerConfig->healthTimeSkipMultiplier = std::clamp(GetIniFloat("Modifiers", "HealthTimeSkipMultiplier", defaultPlayerConfig.healthTimeSkipMultiplier, g_IniPath.c_str()), 0.01f, 1.0f);
    newPlayerConfig->nightDeadEyeMultiplier = std::clamp(GetIniFloat("Modifiers", "NightDeadEyeMultiplier", defaultPlayerConfig.nightDeadEyeMultiplier, g_IniPath.c_str()), 1.0f, 2.0f);
	newPlayerConfig->deadEyeDirtyMultiplier = std::clamp(GetIniFloat("Modifiers", "DeadEyeDirtyMultiplier", defaultPlayerConfig.deadEyeDirtyMultiplier, g_IniPath.c_str()), 1.0f, 1.5f);
    newPlayerConfig->campedMultiplier = std::clamp(GetIniFloat("Modifiers", "CampedMultiplier", defaultPlayerConfig.campedMultiplier, g_IniPath.c_str()), 0.01f, 10.0f);

    newPlayerConfig->jogStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "JogStaminaMultiplier", defaultPlayerConfig.jogStaminaMultiplier, g_IniPath.c_str()), 1.0f, 3.0f);
    newPlayerConfig->sprintStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "SprintStaminaMultiplier", defaultPlayerConfig.sprintStaminaMultiplier, g_IniPath.c_str()), 1.0f, 5.0f);

	newPlayerConfig->mountWalkStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "MountedWalkStaminaMultiplier", defaultPlayerConfig.mountWalkStaminaMultiplier, g_IniPath.c_str()), 0.5f, 1.0f);
    newPlayerConfig->mountTrotStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "MountedTrotStaminaMultiplier", defaultPlayerConfig.mountTrotStaminaMultiplier, g_IniPath.c_str()), 1.0f, 3.0f);
    newPlayerConfig->mountGallopStaminaMultiplier = std::clamp(GetIniFloat("PlayerMovementModifiers", "MountedGallopStaminaMultiplier", defaultPlayerConfig.mountGallopStaminaMultiplier, g_IniPath.c_str()), 1.0f, 5.0f);

    newPlayerConfig->healthTimeSkipFloor = std::clamp(GetIniFloat("Floors", "HealthTimeSkipFloor", defaultPlayerConfig.healthTimeSkipFloor, g_IniPath.c_str()), 1.0f, 15.0f);
    newPlayerConfig->campedHealthFloor = std::clamp(GetIniFloat("Floors", "CampedHealthFloor", defaultPlayerConfig.campedHealthFloor, g_IniPath.c_str()), 5.0f, 10.0f);
    newPlayerConfig->campedStaminaFloor = std::clamp(GetIniFloat("Floors", "CampedStaminaFloor", defaultPlayerConfig.campedStaminaFloor, g_IniPath.c_str()), 1.0f, 10.0f);
    newPlayerConfig->campedDeadEyeFloor = std::clamp(GetIniFloat("Floors", "CampedDeadEyeFloor", defaultPlayerConfig.campedDeadEyeFloor, g_IniPath.c_str()), 1.0f, 10.0f);

    newHorseConfig->baseHealthDecay = std::clamp(GetIniFloat("HorseBaseDecay", "HorseHealthDecayBase", defaultHorseConfig.baseHealthDecay, g_IniPath.c_str()), 1.0f, 100.0f);
    newHorseConfig->baseStaminaDecay = std::clamp(GetIniFloat("HorseBaseDecay", "HorseStaminaDecayBase", defaultHorseConfig.baseStaminaDecay, g_IniPath.c_str()), 1.0f, 100.0f);

	newHorseConfig->grazeStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseGrazeStaminaMultiplier", defaultHorseConfig.grazeStaminaMultiplier, g_IniPath.c_str()), 0.5f, 1.0f);
    newHorseConfig->trotStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseTrotStaminaMultiplier", defaultHorseConfig.trotStaminaMultiplier, g_IniPath.c_str()), 1.0f, 5.0f);
    newHorseConfig->gallopStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseGallopStaminaMultiplier", defaultHorseConfig.gallopStaminaMultiplier, g_IniPath.c_str()), 1.0f, 8.0f);

    newHorseConfig->healthTimeSkipFloor = std::clamp(GetIniFloat("HorseFloors", "HorseHealthTimeSkipFloor", defaultHorseConfig.healthTimeSkipFloor, g_IniPath.c_str()), 1.0f, 15.0f);
    newHorseConfig->campedHealthFloor = std::clamp(GetIniFloat("HorseFloors", "CampedHealthFloor", defaultHorseConfig.campedHealthFloor, g_IniPath.c_str()), 1.0f, 10.0f);
    newHorseConfig->campedStaminaFloor = std::clamp(GetIniFloat("HorseFloors", "CampedStaminaFloor", defaultHorseConfig.campedStaminaFloor, g_IniPath.c_str()), 1.0f, 10.0f);

    switch (newCoreConfig->logLevelRaw) {
	case 0:
        newCoreConfig->currentLogLevel = LogLevel::ConfigOnly;
		break;
	case 1:
        newCoreConfig->currentLogLevel = LogLevel::Standard;
		break;
	case 2:
        newCoreConfig->currentLogLevel = LogLevel::Verbose;
		break;
	case 2105:
        newCoreConfig->currentLogLevel = LogLevel::Dev;
		break;
	default:
        newCoreConfig->currentLogLevel = LogLevel::ConfigOnly; // Fallback to disabled if somehow an invalid value is present
		break;
	}

    g_CoreConfig.store(newCoreConfig, std::memory_order_release);
    g_GeneralConfig.store(newGeneralConfig, std::memory_order_release);
    g_PlayerConfig.store(newPlayerConfig, std::memory_order_release);
    g_HorseConfig.store(newHorseConfig, std::memory_order_release);

	g_StateChangeTickIntervalMs.store(newCoreConfig->stateChangeTickIntervalMs, std::memory_order_release);
    g_StateChangeTickIntervalRestrictedMs.store(static_cast<int>(newCoreConfig->stateChangeTickIntervalMs * newCoreConfig->restrictedStateChangeTickIntervalMultiplier), std::memory_order_release);
    
    g_CurrentLogLevel.store(static_cast<int>(newCoreConfig->logLevelRaw), std::memory_order_release);

    LogConfiguration(newCoreConfig.get(), newGeneralConfig.get(), newPlayerConfig.get(), newHorseConfig.get());
}