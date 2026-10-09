/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <string>
#include <atomic>
#include "CMTypes.h"

enum class LogLevel : int { Disabled = 0, Standard = 1, Verbose = 2, Dev = 2105 };

struct DevLoggingCache {
    bool lastFreezeActive = false;
    bool lastIsAtCamp = false;
    bool lastIsSleeping = false;
    bool lastIsJailed = false;
    int  lastBathingState = 0;
};

extern std::string g_LogPath;

void AsyncLogWriterWorker(std::atomic<bool>& runFlag);

void TryWaitForLogQueueDrain();

void WriteLog(LogLevel requiredLevel, const std::string& message);

void ClearLog();

bool HasDevLogCacheChanged(GameplayContext& ctx);

void UpdateDevLogCache(GameplayContext& ctx);

void ClearDevLogCache();