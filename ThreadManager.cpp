/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <thread>
#include "ThreadManager.h"
#include "Config.h"
#include "Logger.h"

ScriptThreadManager g_ThreadManager;

std::atomic<bool> g_RunWatcherThread(false);
std::condition_variable g_WatcherCv;

void ScriptThreadManager::Initialize() {
    g_RunWatcherThread.store(true, std::memory_order_release);
    watcherThread = std::thread(IniWatcherThread);
}

ScriptThreadManager::~ScriptThreadManager() {
    g_RunWatcherThread.store(false, std::memory_order_release);

    // Notify the condition variable to instantly wake the thread up from its 2-second slumber
    g_WatcherCv.notify_all();
    
    if (watcherThread.joinable()) {
        watcherThread.join();
    }

    StopAsyncLogger();
}