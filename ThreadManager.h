/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>

class ScriptThreadManager {
public:
    std::thread iniWatcherThread;
    std::thread loggerThread;

    ScriptThreadManager() = default;
    ~ScriptThreadManager();

    // Spawns your background threads on script boot
    void Initialize();

    // Safely tears down and joins background threads
    void Shutdown();
};

extern ScriptThreadManager g_ThreadManager;

// INI Watcher synchronization
extern std::atomic<bool> g_RunIniWatcherThread;
extern std::condition_variable g_IniWatcherCv;
extern std::mutex g_IniWatcherCvMutex;

// Logger orchestration tools
extern std::condition_variable g_LogCV; 
extern std::mutex g_LogQueueMutex;