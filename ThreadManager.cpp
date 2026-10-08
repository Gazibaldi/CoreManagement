/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <thread>
#include <windows.h>
#include "ThreadManager.h"
#include "Config.h"
#include "Logger.h"

ScriptThreadManager g_ThreadManager;

// INI Watcher synchronization variables
std::atomic<bool> g_RunIniWatcherThread(false);
std::condition_variable g_IniWatcherCv;
std::mutex g_IniWatcherCvMutex;

// Logger synchronization variables
std::atomic<bool> g_RunLogThread(false);
extern std::condition_variable g_LogCV;
extern std::mutex g_LogQueueMutex;

void IniWatcherThread() {
    WIN32_FILE_ATTRIBUTE_DATA fileData;
    FILETIME lastWriteTime = { 0, 0 };

    if (GetFileAttributesExA(g_IniPath.c_str(), GetFileExInfoStandard, &fileData)) {
        lastWriteTime = fileData.ftLastWriteTime;
    }

    while (g_RunIniWatcherThread) {
        std::unique_lock<std::mutex> lock(g_IniWatcherCvMutex);

        g_IniWatcherCv.wait_for(lock, std::chrono::milliseconds(2000), [&] {
            return !g_RunIniWatcherThread.load(std::memory_order_acquire);
            });

        // Check again immediately after waking up to handle instant shutdown
        if (!g_RunIniWatcherThread) break;

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

void ScriptThreadManager::Initialize() {
    // ALWAYS clean up any existing thread before starting a new one (reloads persist the global manager)
    Shutdown();

    // Boot up the Logger thread first so the script can log safely immediately
    g_RunLogThread.store(true, std::memory_order_release);
    loggerThread = std::thread(AsyncLogWriterWorker, std::ref(g_RunLogThread));

    // Boot up the INI Watcher thread
    g_RunIniWatcherThread.store(true, std::memory_order_release);
    iniWatcherThread = std::thread(IniWatcherThread);
}

void ScriptThreadManager::Shutdown(bool isGameExiting) {
    // Signal the threads to stop
    g_RunIniWatcherThread.store(false, std::memory_order_release);
    g_RunLogThread.store(false, std::memory_order_release);

    // Wake up the condition variables loop in case it's waiting/sleeping
    {
        std::lock_guard<std::mutex> lock(g_IniWatcherCvMutex);
        g_IniWatcherCv.notify_all();
    }
    {
        std::lock_guard<std::mutex> lock(g_LogQueueMutex);
        g_LogCV.notify_all();
    }

    // Wait for the thread to completely finish executing
    if (iniWatcherThread.joinable()) {
        if (isGameExiting) {
            iniWatcherThread.detach();
        }
        else {
            iniWatcherThread.join(); // Safe to join during a reload because thread is fully responsive!
        }
    }

    if (loggerThread.joinable()) {
        if (isGameExiting) {
            TryWaitForLogQueueDrain();

            // Cut the cord. If Windows hasn't frozen the worker thread yet, it will 
            // finish naturally. If it has, detaching prevents RDR2 from deadlocking on exit.
            loggerThread.detach();
        }
        else {
            loggerThread.join(); // Safe to join during a reload because thread is fully responsive!
        }
    }
}