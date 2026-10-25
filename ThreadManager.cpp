/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#include <thread>
#include "ThreadManager.h"
#include "Config.h"
#include "Logger.h"

ScriptThreadManager g_ThreadManager;

void ScriptThreadManager::Initialise() {
    // ALWAYS clean up any existing thread before starting a new one (reloads persist the global manager)
    Shutdown();

	loggerThread = InitialiseLoggerThread();
    configThread = InitialiseConfigThread();
}

void ScriptThreadManager::Shutdown(bool isGameExiting) {
    ShutdownLogger();
	ShutdownConfig();

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