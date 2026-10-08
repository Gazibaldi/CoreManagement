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
#include "ThreadManager.h"

std::string VERSION = "v0.3-ALPHA";

std::mutex g_ConfigMutex;
std::mutex g_WatcherCvMutex;

CoreConfigSettings g_CoreConfig;
GeneralConfigSettings g_GeneralConfig;
PlayerConfigSettings g_PlayerConfig;
HorseConfigSettings g_HorseConfig;

std::atomic<int> g_CurrentLogLevel{ static_cast<int>(LogLevel::Disabled) };

std::atomic<bool> g_ShouldReloadConfig(false);

std::string g_IniPath = "";

std::atomic<int> g_coreDrainTickIntervalMs(5000);
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

void IniWatcherThread() {
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    FILETIME lastWriteTime = { 0, 0 };

    if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
        lastWriteTime = fileData.ftLastWriteTime;
    }

    while (g_RunWatcherThread) {
        std::unique_lock<std::mutex> lock(g_WatcherCvMutex);
        
        g_WatcherCv.wait_for(lock, std::chrono::milliseconds(2000), [] {
            return !g_RunWatcherThread;
        });

        // Check again immediately after waking up to handle instant shutdown
        if (!g_RunWatcherThread) break;

        if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
            if (CompareFileTime(&fileData.ftLastWriteTime, &lastWriteTime) > 0) {
                lastWriteTime = fileData.ftLastWriteTime;

                // Give the OS text editor 100ms to finish flushing its buffer to disk cleanly
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                WriteLog(LogLevel::Standard, "Config change detected. Parsing INI...");

                {
                    std::lock_guard<std::mutex> lock(g_ConfigMutex);
                    LoadConfiguration(); // Overwrites g_CoreConfig, g_GeneralConfig, etc.
                }

                g_ShouldReloadConfig.store(true, std::memory_order_release);
                WriteLog(LogLevel::Standard, "Configuration dynamically reloaded.");
            }
        }
    }
}

int ValidateLogLevel(int value, int defaultValue) {
	if (value == 0 || value == 1 || value == 2 || value == 2105) // 2105 is a hardcoded dev log level for very verbose debugging, not intended for gameplay
        return value;

    return defaultValue;
}

void BuildDefaultConfigFile() {
    std::ofstream outFile(g_IniPath, std::ios::out | std::ios::trunc);
    if (outFile.is_open()) {
        outFile << "; CoreManagement.ini " << VERSION << "\n\n"
            << "[Performance]\n"
            << "; How often the main drain game simulation thread ticks (in milliseconds)\n"
            << "; Minimum interval is 5 seconds (5000 ms) max is 5 mins (300000 ms)\n"
            << "coreDrainTickIntervalMs = " << g_CoreConfig.coreDrainTickIntervalMs << "\n"
            << "; How often the main state simulation should run (in milliseconds). \n"
            << "; This should be low to capture mission triggers etc. The drain will be forced  in such cases\n"
            << "; Just leave it at 500 unless it causes performance issues\n"
            << "; Adjust log level first though as that is definitely the first place to gain performance\n"
            << "stateChangeTickIntervalMs = " << g_CoreConfig.stateChangeTickIntervalMs << "\n\n"

            << "[Logging]\n"
            << "; 0 = Disabled (Max performance), 1 = Standard, 2 = Verbose (Not recommended for normal play)\n"
            << "LogLevel = " << g_CoreConfig.logLevelRaw << "\n\n"

            << "[General]\n"
            << "; in-game hour in which night officially begins\n"
            << "NightStartHour = " << g_GeneralConfig.nightStartHour << "\n"
            << "; in-game hour in which day officially begins\n"
            << "NightEndHour = " << g_GeneralConfig.nightEndHour << "\n"
            << "; If 1 (true), core drain ticks continuously during story/world missions. \n"
            << "; No catch-up math will trigger on mission completion.\n"
            << "AllowDrainInMissions = " << static_cast<int>(g_GeneralConfig.allowDrainInMissions) << "\n"
            << "; If 1 (true), core drain ticks continuously while sitting at poker, blackjack, etc.\n"
            << "; No catch-up math will trigger when standing up from the table.\n"
            << "AllowDrainInMinigames = " << static_cast<int>(g_GeneralConfig.allowDrainInMinigames) << "\n\n"

            << "[BaseDecayAwake]\n"
            << "; Points of core decay per in-game hour\n"
            << "HealthDecayBase = " << g_PlayerConfig.baseHealthDecay << "\n"
            << "StaminaDecayBase = " << g_PlayerConfig.baseStaminaDecay << "\n"
            << "DeadEyeDecayBase = " << g_PlayerConfig.baseDeadEyeDecay << "\n\n"

            << "[Modifiers]\n"
            << "; percentage based modifiers for decay points above (0.5 = 50% drain [5 decay becomes 2.5 decay])\n"
            << "; SleepHealthMultiplier  - Decay rate multiplier for HEALTH when the player is asleep\n"
            << "; NightDeadEyeMultiplier - Decay rate multiplier for the DEAD EYE during the night hours (NightStartHour -> NightEndHour)\n"
            << "; RestrainedMultiplier   - Decay rate multiplier for the all cores if the player is in a restrained state (e.g., camp/jail/bath/barber)\n"
            << ";                          In bathing scenarios this is just your HEALTH as bathing is rewarding for STAMINA and DEAD EYE\n"
            << "SleepHealthMultiplier = " << g_PlayerConfig.sleepHealthMultiplier << "\n"
            << "NightDeadEyeMultiplier = " << g_PlayerConfig.nightDeadEyeMultiplier << "\n"
            << "RestrainedMultiplier = " << g_PlayerConfig.restrainedMultiplier << "\n\n"

            << "[Floors]\n"
            << "; minimum core levels in the event of a time skip (missions, minigames, sleep).\n"
            << "; is inactive for the corresponding gameplay type if AllowDrainInMissions or AllowDrainInMinigames is active. Always active for sleep and other events\n"
            << "; in general gameplay these are ignored and you can drop to zero core\n"
            << "; for example, when you sleep for 12 hours you will awaken with minimum cores of:\n"
			<< "; Health - 15.0\n" // todo: add stamina and dead eye values here too, as well as a non -sleep example for the player to understand the difference between sleep and non-sleep time skips
            << "HealthTimeSkipFloor = " << g_PlayerConfig.healthTimeSkipFloor << "\n"
            << "RestrainedHealthFloor = " << g_PlayerConfig.restrainedHealthFloor << "\n"
            << "RestrainedStaminaFloor = " << g_PlayerConfig.restrainedStaminaFloor << "\n"
            << "RestrainedDeadEyeFloor = " << g_PlayerConfig.restrainedDeadEyeFloor << "\n\n"

            << "[HorseBaseDecay]\n"
            << "; Points decay per in-game hour\n"
            << "HorseHealthDecayBase = " << g_HorseConfig.baseHealthDecay << "\n"
            << "HorseStaminaDecayBase = " << g_HorseConfig.baseStaminaDecay << "\n\n"

            << "[HorseMovementModifiers]\n"
            << "; percentage based modifiers for decay points above (0.5 = 50% drain [5 decay becomes 2.5 decay]) depending movement speed of mount\n"
            << "HorseTrotStaminaMultiplier = " << g_HorseConfig.trotStaminaMultiplier << "\n"
            << "HorseGallopStaminaMultiplier = " << g_HorseConfig.gallopStaminaMultiplier << "\n\n"

            << "[HorseFloors]\n"
            << "; minimum core levels in the event of a time skip (missions, minigames, sleep).\n"
            << "; is inactive for the corresponding gameplay type if AllowDrainInMissions or AllowDrainInMinigames is active. Always active for sleep and other events\n"
            << "; in general gameplay these are ignored and you can drop to 0.0\n"
            << "RestrainedHorseHealthFloor = " << g_HorseConfig.restrainedHealthFloor << "\n"
            << "RestrainedHorseStaminaFloor = " << g_HorseConfig.restrainedStaminaFloor << "\n";
        outFile.close();
    }
}

void LogConfiguration() {    
    std::ostringstream table;
    table << std::fixed << std::setprecision(2)
        << "\n\n+---------------------------------------+------------+\n"
        << "| Configuration Parameter               |      Value |\n"
        << "+---------------------------------------+------------+\n"
        << "| Performance: Drain Tick Interval (ms) | " << std::setw(10) << g_CoreConfig.coreDrainTickIntervalMs << " |\n"
        << "| Performance: State Tick Interval (ms) | " << std::setw(10) << g_CoreConfig.stateChangeTickIntervalMs << " |\n"
        << "| Logging: Log Level                    | " << std::setw(10) << g_CoreConfig.logLevelRaw << " |\n"
        << "| General: Night Start Hour             | " << std::setw(10) << g_GeneralConfig.nightStartHour << " |\n"
        << "| General: Night End Hour               | " << std::setw(10) << g_GeneralConfig.nightEndHour << " |\n"
        << "| General: Allow Drain in Missions      | " << std::setw(10) << static_cast<int>(g_GeneralConfig.allowDrainInMissions) << " |\n"
        << "| General: Allow Drain in Minigames     | " << std::setw(10) << static_cast<int>(g_GeneralConfig.allowDrainInMinigames) << " |\n"
        << "| Player: Health Base Awake Decay       | " << std::setw(10) << g_PlayerConfig.baseHealthDecay << " |\n"
        << "| Player: Stamina Base Awake Decay      | " << std::setw(10) << g_PlayerConfig.baseStaminaDecay << " |\n"
        << "| Player: Dead Eye Base Awake Decay     | " << std::setw(10) << g_PlayerConfig.baseDeadEyeDecay << " |\n"
        << "| Player: Sleep Health Multiplier       | " << std::setw(10) << g_PlayerConfig.sleepHealthMultiplier << " |\n"
        << "| Player: Night Dead Eye Multiplier     | " << std::setw(10) << g_PlayerConfig.nightDeadEyeMultiplier << " |\n"
        << "| Player: Restrained Multiplier         | " << std::setw(10) << g_PlayerConfig.restrainedMultiplier << " |\n"
        << "| Player: Health Sleep Floor            | " << std::setw(10) << g_PlayerConfig.healthTimeSkipFloor << " |\n"
        << "| Player: Health Restrained Floor       | " << std::setw(10) << g_PlayerConfig.restrainedHealthFloor << " |\n"
        << "| Player: Stamina Restrained Floor      | " << std::setw(10) << g_PlayerConfig.restrainedStaminaFloor << " |\n"
        << "| Player: Dead Eye Restrained Floor     | " << std::setw(10) << g_PlayerConfig.restrainedDeadEyeFloor << " |\n"
        << "| Horse: Health Base Decay              | " << std::setw(10) << g_HorseConfig.baseHealthDecay << " |\n"
        << "| Horse: Stamina Base Decay             | " << std::setw(10) << g_HorseConfig.baseStaminaDecay << " |\n"
        << "| Horse: Stamina Trot Multiplier        | " << std::setw(10) << g_HorseConfig.trotStaminaMultiplier << " |\n"
        << "| Horse: Stamina Gallop Multiplier      | " << std::setw(10) << g_HorseConfig.gallopStaminaMultiplier << " |\n"
        << "| Horse: Health Restrained Floor        | " << std::setw(10) << g_HorseConfig.restrainedHealthFloor << " |\n"
        << "| Horse: Stamina Restrained Floor       | " << std::setw(10) << g_HorseConfig.restrainedStaminaFloor << " |\n"
        << "+---------------------------------------+------------+";

    WriteLog(LogLevel::Verbose, "Internal Parameter Map Hydrated:" + table.str());
}

void LoadConfiguration() {

    if (!DoesFileExist(g_IniPath)) {
        BuildDefaultConfigFile();

        WriteLog(LogLevel::Standard, "Configuration file missing. Generating fresh defaults at: " + g_IniPath);
    }

    WriteLog(LogLevel::Standard, "Loading ini configuration from: " + g_IniPath);

    g_CoreConfig.coreDrainTickIntervalMs = std::clamp(GetIniInt("Performance", "coreDrainTickIntervalMs", 10000, g_IniPath.c_str()), 5000, 300000); // minimum 5 second tick interval maximum 5 minute tick interval
		                                                                                                                                // it's designed to be a slow loop as core drain 
                                                                                                                                        // isn't really a real-time process        
        
	g_CoreConfig.stateChangeTickIntervalMs = std::clamp(GetIniInt("Performance", "stateChangeTickIntervalMs", 500, g_IniPath.c_str()), 500, 2000); // clamp between 0.5 and 2 second tick interval 
                                                                                                                                                    // it's designed to be a fast loop, but not too fast 
                                                                                                                                                    // to avoid excessive CPU usage while not too slow to miss state changes

	g_CoreConfig.logLevelRaw = ValidateLogLevel(GetIniInt("Logging", "LogLevel", 1, g_IniPath.c_str()), 1); // minimum log level is 0 (disabled) max is a hardcoded 2105 (dev- very verbose not for gameplay). 
                                                                                                            // Range is 0-2 for normal gameplay. 0=disabled, 1=standard, 2=verbose
    g_GeneralConfig.nightStartHour = std::clamp(GetIniInt("General", "NightStartHour", 19, g_IniPath.c_str()), 0, 23); // clamp between 0 and 23
    g_GeneralConfig.nightEndHour = std::clamp(GetIniInt("General", "NightEndHour", 7, g_IniPath.c_str()), 0, 23); // clamp between 0 and 23
    g_GeneralConfig.allowDrainInMissions = GetIniBool("General", "AllowDrainInMissions", false, g_IniPath.c_str());
    g_GeneralConfig.allowDrainInMinigames = GetIniBool("General", "AllowDrainInMinigames", false, g_IniPath.c_str());

    g_PlayerConfig.baseHealthDecay = std::clamp(GetIniFloat("BaseDecayAwake", "HealthDecayBase", 3.5f, g_IniPath.c_str()), 1.0f, 100.0f);
    g_PlayerConfig.baseStaminaDecay = std::clamp(GetIniFloat("BaseDecayAwake", "StaminaDecayBase", 5.5f, g_IniPath.c_str()), 1.0f, 100.0f);
    g_PlayerConfig.baseDeadEyeDecay = std::clamp(GetIniFloat("BaseDecayAwake", "DeadEyeDecayBase", 4.16f, g_IniPath.c_str()), 1.0f, 100.0f);
    g_PlayerConfig.sleepHealthMultiplier = std::clamp(GetIniFloat("Modifiers", "SleepHealthMultiplier", 0.5f, g_IniPath.c_str()), 0.01f, 10.0f);
    g_PlayerConfig.nightDeadEyeMultiplier = std::clamp(GetIniFloat("Modifiers", "NightDeadEyeMultiplier", 1.3f, g_IniPath.c_str()), 1.0f, 10.0f);
    g_PlayerConfig.restrainedMultiplier = std::clamp(GetIniFloat("Modifiers", "RestrainedMultiplier", 0.25f, g_IniPath.c_str()), 0.01f, 10.0f);
    g_PlayerConfig.healthTimeSkipFloor = std::clamp(GetIniFloat("Floors", "HealthTimeSkipFloor", 15.0f, g_IniPath.c_str()), 1.0f, 15.0f);
    g_PlayerConfig.restrainedHealthFloor = std::clamp(GetIniFloat("Floors", "RestrainedHealthFloor", 5.0f, g_IniPath.c_str()), 5.0f, 10.0f);
    g_PlayerConfig.restrainedStaminaFloor = std::clamp(GetIniFloat("Floors", "RestrainedStaminaFloor", 5.0f, g_IniPath.c_str()), 1.0f, 10.0f);
    g_PlayerConfig.restrainedDeadEyeFloor = std::clamp(GetIniFloat("Floors", "RestrainedDeadEyeFloor", 5.0f, g_IniPath.c_str()), 1.0f, 10.0f);

    g_HorseConfig.baseHealthDecay = std::clamp(GetIniFloat("HorseBaseDecay", "HorseHealthDecayBase", 3.5f, g_IniPath.c_str()), 1.0f, 100.0f);
    g_HorseConfig.baseStaminaDecay = std::clamp(GetIniFloat("HorseBaseDecay", "HorseStaminaDecayBase", 5.5f, g_IniPath.c_str()), 1.0f, 100.0f);
    g_HorseConfig.trotStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseTrotStaminaMultiplier", 1.3f, g_IniPath.c_str()), 1.0f, 10.0f);
    g_HorseConfig.gallopStaminaMultiplier = std::clamp(GetIniFloat("HorseMovementModifiers", "HorseGallopStaminaMultiplier", 4.5f, g_IniPath.c_str()), 1.0f, 10.0f);
    g_HorseConfig.restrainedHealthFloor = std::clamp(GetIniFloat("HorseFloors", "RestrainedHealthFloor", 5.0f, g_IniPath.c_str()), 1.0f, 10.0f);
    g_HorseConfig.restrainedStaminaFloor = std::clamp(GetIniFloat("HorseFloors", "RestrainedStaminaFloor", 5.0f, g_IniPath.c_str()), 1.0f, 10.0f);

    if (g_CoreConfig.logLevelRaw == static_cast<int>(LogLevel::Dev)) g_CoreConfig.currentLogLevel = LogLevel::Dev;
    else if (g_CoreConfig.logLevelRaw == static_cast<int>(LogLevel::Verbose)) g_CoreConfig.currentLogLevel = LogLevel::Verbose;
    else if (g_CoreConfig.logLevelRaw == static_cast<int>(LogLevel::Standard)) g_CoreConfig.currentLogLevel = LogLevel::Standard;
    else g_CoreConfig.currentLogLevel = LogLevel::Disabled;

    g_coreDrainTickIntervalMs.store(g_CoreConfig.coreDrainTickIntervalMs);
	g_stateChangeTickIntervalMs.store(g_CoreConfig.stateChangeTickIntervalMs);

    g_CurrentLogLevel.store(static_cast<int>(g_CoreConfig.logLevelRaw), std::memory_order_release);

    if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Verbose))
        LogConfiguration();
}