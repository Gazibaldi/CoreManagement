#pragma once
#include "Logger.h"
#include <mutex>

// Explicit extern linkage to look up your main configuration settings
struct CoreConfigSettings {
    int mainLoopTickIntervalMs = 1000;
    int logLevelRaw = 1;

    LogLevel currentLogLevel = LogLevel::Standard;
};
extern CoreConfigSettings g_CoreConfig;
extern std::mutex g_ConfigMutex;
