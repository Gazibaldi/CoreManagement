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

// Instantiate the actual memory addresses for your extern variables
std::atomic<bool> g_RunLogThread(true);
std::string g_LogPath = "";

// Private, file-isolated synchronization variables (not exposed to main.cpp)
static std::queue<std::string> s_LogQueue;
static std::mutex s_LogQueueMutex;
static std::condition_variable s_LogCV;
static std::thread s_LogWorkerThread;

static void AsyncLogWriterWorker() {
    while (g_RunLogThread || !s_LogQueue.empty()) {
        std::unique_lock<std::mutex> lock(s_LogQueueMutex);

        s_LogCV.wait_for(lock, std::chrono::milliseconds(2000), [] {
            return !s_LogQueue.empty() || !g_RunLogThread;
            });

        // If we were woken up to shut down and the queue is completely empty, 
        // break instantly to skip the slow disk I/O operations entirely.
        if (!g_RunLogThread && s_LogQueue.empty()) {
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

void StartAsyncLogger() {
    g_RunLogThread = true;
    s_LogWorkerThread = std::thread(AsyncLogWriterWorker);
}

void StopAsyncLogger() {
    g_RunLogThread = false;
    s_LogCV.notify_all();
    if (s_LogWorkerThread.joinable()) {
        s_LogWorkerThread.join();
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
        std::lock_guard<std::mutex> lock(s_LogQueueMutex);
        s_LogQueue.push(formattedMessage);
    }
    s_LogCV.notify_one();
}