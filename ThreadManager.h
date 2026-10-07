/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <thread>
#include <mutex>
#include <atomic>

class ScriptThreadManager {
public:
    std::thread watcherThread;

    // Default constructor and destructor
    ScriptThreadManager() = default;
    ~ScriptThreadManager();

    // Spawns your background threads on script boot
    void Initialize();
};

// Expose the global instance to main.cpp and other files
extern ScriptThreadManager g_ThreadManager;

extern std::atomic<bool> g_RunWatcherThread;
extern std::condition_variable g_WatcherCv;
extern std::mutex g_WatcherCvMutex;