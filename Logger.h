/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <string_view>
#include <string>
#include <atomic>
#include "CMTypes.h"

#ifdef DEVTEST
#include <format>

// __VA_OPT__(,) safely deletes the comma if you pass 0 formatting variables!
#define LOG_DEV(fmt_str, ...) \
        do { \
            if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev)) \
                WriteLog(LogLevel::Dev, std::format(fmt_str __VA_OPT__(, __VA_ARGS__))); \
        } while(0)
#else
#define LOG_DEV(fmt_str, ...) do { } while(0)
#endif // DEVTEST

enum class LogLevel : int { Disabled = 0, Standard = 1, Verbose = 2, Dev = 2105 };

struct DevLoggingCache {
    bool lastFreezeActive = false;
    bool lastIsAtCamp = false;
    bool lastIsSleeping = false;
    bool lastIsJailed = false;
    bool lastIsBathing = false;
	bool lastIsInMission = false;
	bool lastIsInMinigame = false;
};

extern std::string g_LogPath;
extern std::atomic<int> g_CurrentLogLevel;

void AsyncLogWriterWorker(std::atomic<bool>& runFlag);

void TryWaitForLogQueueDrain();

void WriteLog(LogLevel requiredLevel, const std::string_view message);

void ClearLog();

bool HasDevLogCacheChanged(const GameplayContext& ctx);

void UpdateDevLogCache(const GameplayContext& ctx);

void ClearDevLogCache();