/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <windows.h>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <fstream>
#include "Config.h"
#include "Logger.h"

std::string g_LogPath = "";

// Shared orchestration variables instantiated here
std::mutex g_LogQueueMutex;
std::condition_variable g_LogCV;

// Private, file-isolated synchronization variables (not exposed to script.cpp)
static std::queue<std::string> s_LogQueue;

DevLoggingCache g_LogCache;

void AsyncLogWriterWorker(std::atomic<bool>& runFlag) {
    while (runFlag || !s_LogQueue.empty()) {
        std::unique_lock<std::mutex> lock(g_LogQueueMutex);

        g_LogCV.wait_for(lock, std::chrono::milliseconds(2000), [&] {
            return !s_LogQueue.empty() || !runFlag.load(std::memory_order_acquire);
            });

        // If we were woken up to shut down and the queue is completely empty, 
        // break instantly to skip the slow disk I/O operations entirely.
        if (!runFlag.load(std::memory_order_acquire) && s_LogQueue.empty()) {
            break;
        }

        std::ofstream logFile(g_LogPath, std::ios_base::app);
        if (logFile.is_open()) {
            while (!s_LogQueue.empty()) {
                logFile << s_LogQueue.front() << "\n";
                s_LogQueue.pop();
            }
            logFile.flush();
            logFile.close();
        }
    }
}

void WriteLog(LogLevel requiredLevel, const std::string& message) {
    int systemLevel = g_CurrentLogLevel.load(std::memory_order_acquire);
    if (static_cast<int>(systemLevel) < static_cast<int>(requiredLevel)) return;

    SYSTEMTIME st;
    GetLocalTime(&st);
    char timeBuffer[16];
    sprintf_s(timeBuffer, "[%02d:%02d:%02d] ", st.wHour, st.wMinute, st.wSecond);
    std::string formattedMessage = std::string(timeBuffer) + message;

    {
        std::lock_guard<std::mutex> lock(g_LogQueueMutex);
        s_LogQueue.push(formattedMessage);
    }
    g_LogCV.notify_one();
}

void ClearLog() {
    std::ofstream(g_LogPath, std::ios::out | std::ios::trunc).close();
}

bool HasDevLogCacheChanged(GameplayContext& ctx) {
	return (ctx.freezeActive != g_LogCache.lastFreezeActive ||
		ctx.isAtCamp != g_LogCache.lastIsAtCamp ||
		ctx.isSleeping != g_LogCache.lastIsSleeping ||
		ctx.isJailed != g_LogCache.lastIsJailed ||
		ctx.bathingState != g_LogCache.lastBathingState);
}

void UpdateDevLogCache(GameplayContext& ctx) {
	g_LogCache.lastFreezeActive = ctx.freezeActive;
	g_LogCache.lastIsAtCamp = ctx.isAtCamp;
	g_LogCache.lastIsSleeping = ctx.isSleeping;
	g_LogCache.lastIsJailed = ctx.isJailed;
	g_LogCache.lastBathingState = ctx.bathingState;
}

void ClearDevLogCache() {
	g_LogCache.lastFreezeActive = false;
	g_LogCache.lastIsAtCamp = false;
	g_LogCache.lastIsSleeping = false;
	g_LogCache.lastIsJailed = false;
	g_LogCache.lastBathingState = 0;
}