#include <thread>
#include "ThreadManager.h"
#include "Config.h"
#include "Logger.h"

ScriptThreadManager g_ThreadManager;

void ScriptThreadManager::Initialize() {
    g_RunWatcherThread.store(true, std::memory_order_release);
    watcherThread = std::thread(IniWatcherThread);
}

ScriptThreadManager::~ScriptThreadManager() {
    g_RunWatcherThread.store(false, std::memory_order_release);
    
    if (watcherThread.joinable()) {
        watcherThread.join();
    }

    StopAsyncLogger();
}