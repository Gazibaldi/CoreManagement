#include <windows.h>
#include <iomanip>
#include <string>
#include <sstream>
#include <charconv>
#include <system_error>
#include "Config.h"

std::mutex g_ConfigMutex;
CoreConfigSettings g_CoreConfig;
GeneralConfigSettings g_GeneralConfig;
PlayerConfigSettings g_PlayerConfig;
HorseConfigSettings g_HorseConfig;

std::atomic<int> g_CurrentLogLevel{ static_cast<int>(LogLevel::Standard) };
std::atomic<bool> g_RunWatcherThread(false);
std::atomic<bool> g_ShouldReloadConfig(false);
std::string g_IniPath = "";
std::atomic<int> g_TickIntervalMs(1000);

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
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
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

void LoadConfiguration() {

    if (!DoesFileExist(g_IniPath)) {
        WriteLog(LogLevel::Standard, "Configuration file not found at: " + g_IniPath);
        WriteLog(LogLevel::Standard, "Loading default configuration file...");
    }

    {
        std::lock_guard<std::mutex> lock(g_ConfigMutex);

        g_CoreConfig.mainLoopTickIntervalMs = MAX(GetIniInt("Performance", "MainLoopTickIntervalMs", 1000, g_IniPath.c_str()), 1000); // minimum 1 second tick interval
        g_CoreConfig.logLevelRaw = MIN(MAX(GetIniInt("Logging", "LogLevel", 1, g_IniPath.c_str()), 0), 2); // clamp between 0 and 2 
        g_GeneralConfig.nightStartHour = MIN(MAX(GetIniInt("General", "NightStartHour", 19, g_IniPath.c_str()), 0), 23); // clamp between 0 and 23
        g_GeneralConfig.nightEndHour = MIN(MAX(GetIniInt("General", "NightEndHour", 7, g_IniPath.c_str()), 0), 23); // clamp between 0 and 23
        g_GeneralConfig.allowDrainInMissions = GetIniBool("General", "AllowDrainInMissions", false, g_IniPath.c_str());
        g_GeneralConfig.allowDrainInMinigames = GetIniBool("General", "AllowDrainInMinigames", false, g_IniPath.c_str());

        g_PlayerConfig.baseHealthDecay = MIN(MAX(GetIniFloat("BaseDecayAwake", "HealthDecayBase", 3.5f, g_IniPath.c_str()), 1.0f), 100.0f);
        g_PlayerConfig.baseStaminaDecay = MIN(MAX(GetIniFloat("BaseDecayAwake", "StaminaDecayBase", 5.5f, g_IniPath.c_str()), 1.0f), 100.0f);
        g_PlayerConfig.baseDeadEyeDecay = MIN(MAX(GetIniFloat("BaseDecayAwake", "DeadEyeDecayBase", 4.16f, g_IniPath.c_str()), 1.0f), 100.0f);
        g_PlayerConfig.sleepHealthMultiplier = MIN(MAX(GetIniFloat("Modifiers", "SleepHealthMultiplier", 0.5f, g_IniPath.c_str()), 0.01f), 10.0f);
        g_PlayerConfig.nightDeadEyeMultiplier = MIN(MAX(GetIniFloat("Modifiers", "NightDeadEyeMultiplier", 1.5f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_PlayerConfig.weaponWheelMultiplier = MIN(MAX(GetIniFloat("Modifiers", "WeaponWheelMultiplier", 0.05f, g_IniPath.c_str()), 0.01f), 10.0f);
        g_PlayerConfig.campMultiplier = MIN(MAX(GetIniFloat("Modifiers", "CampMultiplier", 0.25f, g_IniPath.c_str()), 0.01f), 10.0f);
        g_PlayerConfig.healthSleepFloor = MIN(MAX(GetIniFloat("Floors", "HealthSleepFloor", 15.0f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_PlayerConfig.healthAwakeFloor = MIN(MAX(GetIniFloat("Floors", "HealthAwakeFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_PlayerConfig.staminaFloor = MIN(MAX(GetIniFloat("Floors", "StaminaFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_PlayerConfig.deadEyeFloor = MIN(MAX(GetIniFloat("Floors", "DeadEyeFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);

        g_HorseConfig.baseHealthDecay = MIN(MAX(GetIniFloat("HorseBaseDecay", "HorseHealthDecayBase", 3.5f, g_IniPath.c_str()), 1.0f), 100.0f);
        g_HorseConfig.baseStaminaDecay = MIN(MAX(GetIniFloat("HorseBaseDecay", "HorseStaminaDecayBase", 5.5f, g_IniPath.c_str()), 1.0f), 100.0f);
        g_HorseConfig.trotStaminaMultiplier = MIN(MAX(GetIniFloat("HorseMovementModifiers", "HorseTrotStaminaMultiplier", 1.3f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_HorseConfig.gallopStaminaMultiplier = MIN(MAX(GetIniFloat("HorseMovementModifiers", "HorseGallopStaminaMultiplier", 4.5f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_HorseConfig.healthFloor = MIN(MAX(GetIniFloat("HorseFloors", "HorseHealthFloor", 5.0f, g_IniPath.c_str()), 1.0f), 10.0f);
        g_HorseConfig.staminaFloor = MIN(MAX(GetIniFloat("HorseFloors", "HorseStaminaFloor", 1.0f, g_IniPath.c_str()), 1.0f), 10.0f);

        if (g_CoreConfig.logLevelRaw >= 2) g_CoreConfig.currentLogLevel = LogLevel::Dev;
        else if (g_CoreConfig.logLevelRaw == 1) g_CoreConfig.currentLogLevel = LogLevel::Standard;
        else g_CoreConfig.currentLogLevel = LogLevel::Disabled;

        g_TickIntervalMs.store(g_CoreConfig.mainLoopTickIntervalMs);
        g_CurrentLogLevel.store(static_cast<int>(g_CoreConfig.logLevelRaw), std::memory_order_release);
    }

    if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev)) {
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
            << "| Player: Health Base Awake Decay    | " << std::setw(10) << g_PlayerConfig.baseHealthDecay << " |\n"
            << "| Player: Stamina Base Awake Decay   | " << std::setw(10) << g_PlayerConfig.baseStaminaDecay << " |\n"
            << "| Player: Dead Eye Base Awake Decay  | " << std::setw(10) << g_PlayerConfig.baseDeadEyeDecay << " |\n"
            << "| Player: Sleep Health Multiplier    | " << std::setw(10) << g_PlayerConfig.sleepHealthMultiplier << " |\n"
            << "| Player: Night Dead Eye Multiplier  | " << std::setw(10) << g_PlayerConfig.nightDeadEyeMultiplier << " |\n"
            << "| Player: Weapon Wheel Multiplier    | " << std::setw(10) << g_PlayerConfig.weaponWheelMultiplier << " |\n"
            << "| Player: Camp Multiplier            | " << std::setw(10) << g_PlayerConfig.campMultiplier << " |\n"
            << "| Player: Health Sleep Floor         | " << std::setw(10) << g_PlayerConfig.healthSleepFloor << " |\n"
            << "| Player: Health Awake Floor         | " << std::setw(10) << g_PlayerConfig.healthAwakeFloor << " |\n"
            << "| Player: Stamina Floor              | " << std::setw(10) << g_PlayerConfig.staminaFloor << " |\n"
            << "| Player: Dead Eye Floor             | " << std::setw(10) << g_PlayerConfig.deadEyeFloor << " |\n"
            << "| Horse: Health Base Decay           | " << std::setw(10) << g_HorseConfig.baseHealthDecay << " |\n"
            << "| Horse: Stamina Base Decay          | " << std::setw(10) << g_HorseConfig.baseStaminaDecay << " |\n"
            << "| Horse: Stamina Trot Multiplier     | " << std::setw(10) << g_HorseConfig.trotStaminaMultiplier << " |\n"
            << "| Horse: Stamina Gallop Multiplier   | " << std::setw(10) << g_HorseConfig.gallopStaminaMultiplier << " |\n"
            << "| Horse: Health Floor                | " << std::setw(10) << g_HorseConfig.healthFloor << " |\n"
            << "| Horse: Stamina Floor               | " << std::setw(10) << g_HorseConfig.staminaFloor << " |\n"
            << "+------------------------------------+------------+";

        WriteLog(LogLevel::Dev, "Loading ini configuration from: " + g_IniPath);
        WriteLog(LogLevel::Dev, "Internal Parameter Map Hydrated:" + table.str());
    }
}