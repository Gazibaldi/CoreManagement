/*
        Copyright © 2026 Gary Tweddle / Gazibaldi.This program is free software : you can redistribute it and /or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or any later version.
*/

#pragma once
#include <thread>

class ThreadManager {
    std::thread configThread;
    std::thread loggerThread;
	std::thread saveManagerThread;
public:
    ThreadManager() = default;
    ~ThreadManager() = default;

    void Initialise();

    void Shutdown(bool isGameExiting = false);
};

extern ThreadManager g_ThreadManager;