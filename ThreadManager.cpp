/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include "ThreadManager.h"
#include "Config.h"
#include "Logger.h"

ScriptThreadManager g_ThreadManager;

void ScriptThreadManager::Initialise() {
    // ALWAYS clean up any existing thread before starting a new one (reloads persist the global manager)
    Shutdown();

    // Boot up the Logger thread first so the script can log safely immediately
    g_RunLogThread.store(true, std::memory_order_release);
    loggerThread = std::thread(AsyncLogWriterWorker, std::ref(g_RunLogThread));

    // Boot up the INI Watcher thread
    g_RunConfigThread.store(true, std::memory_order_release);
    configThread = std::thread(ConfigWatcherWorker);
}

void ScriptThreadManager::Shutdown(bool isGameExiting) {
    // Signal the threads to stop
    g_RunConfigThread.store(false, std::memory_order_release);
    g_RunLogThread.store(false, std::memory_order_release);

    // Wake up the condition variables loop in case it's waiting/sleeping
    {
        std::lock_guard<std::mutex> lock(g_ConfigMutex);
        g_ConfigThreadCV.notify_all();
    }
    {
        std::lock_guard<std::mutex> lock(g_LogQueueMutex);
        g_LogCV.notify_all();
    }

    // Wait for the thread to completely finish executing
    if (configThread.joinable()) {
        if (isGameExiting) 
            configThread.detach();
        else
            configThread.join(); // Safe to join during a reload because thread is fully responsive!
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