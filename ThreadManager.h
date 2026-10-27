/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <thread>

class ScriptThreadManager {
    std::thread configThread;
    std::thread loggerThread;
public:
    ScriptThreadManager() = default;
    ~ScriptThreadManager() = default;

    void Initialise();

    void Shutdown(bool isGameExiting = false);
};

extern ScriptThreadManager g_ThreadManager;