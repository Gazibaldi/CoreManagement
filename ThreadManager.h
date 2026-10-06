#pragma once
#include <thread>

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