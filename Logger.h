/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <string_view>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <format>
#include "CMTypes.h"


#ifdef DEVTEST
// __VA_OPT__(,) safely deletes the comma if you pass 0 formatting variables!
#define LOG_DEV(fmt_str, ...) \
        do { \
            if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(LogLevel::Dev)) \
                WriteLog(LogLevel::Dev, std::format(fmt_str __VA_OPT__(, __VA_ARGS__))); \
        } while(0)
#else
#define LOG_DEV(fmt_str, ...) do { } while(0)
#endif // DEVTEST

#define LOG(level, fmt_str, ...) \
        do { \
            if (g_CurrentLogLevel.load(std::memory_order_acquire) == static_cast<int>(level)) \
                WriteLog(level, std::format(fmt_str __VA_OPT__(, __VA_ARGS__))); \
        } while(0)

enum class LogLevel : int { Disabled = 0, Standard = 1, Verbose = 2, Dev = 2105 };
extern std::atomic<int> g_CurrentLogLevel;

extern std::atomic<bool> g_RunLogThread;
extern std::condition_variable g_LogCV;
extern std::mutex g_LogQueueMutex;

void AsyncLogWriterWorker(std::atomic<bool>& runFlag);

void TryWaitForLogQueueDrain();

void WriteLog(LogLevel requiredLevel, const std::string_view message);

void ClearLog();

bool HasDevLogCacheChanged(const GameplayContext& ctx);

void UpdateDevLogCache(const GameplayContext& ctx);

void ClearDevLogCache();