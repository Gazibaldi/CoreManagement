#include <fstream>
#include <string>
#include <cstdio>
#include "Session.h"
#include "Logger.h"

std::string g_SessionPath = "";

void PrepareNewSessionLogFile() {
    // If the file DOES NOT exist, it's a true Cold Boot from desktop
    std::ifstream sessionCheck(g_SessionPath);
    bool isColdBoot = !sessionCheck.is_open();

    if (sessionCheck.is_open())
        sessionCheck.close();

    if (isColdBoot) {
        ClearLog(); // Clean slate only on cold boot
        WriteLog(LogLevel::Standard, "==== Core Drain Management Engine - COLD DESKTOP BOOT ====");
    }
    else {
        WriteLog(LogLevel::Standard, "==== Core Drain Management Engine - MID-GAME SAVE RELOAD ====");
    }
}

void TryCreateSessionMarkerFile() {
    std::ifstream checkSession(g_SessionPath);

    if (!checkSession.is_open()) {
        std::ofstream createSession(g_SessionPath);
        WriteLog(LogLevel::Verbose, "Session marker file created: " + g_SessionPath);

        if (createSession.is_open())
            createSession.close();
    }
    else {
        checkSession.close();
    }
}

void DeleteSessionMarkerFile() {
    if (std::remove(g_SessionPath.c_str()) != 0)
        WriteLog(LogLevel::Verbose, "Failed to delete session marker file: " + g_SessionPath);
    else
        WriteLog(LogLevel::Verbose, "Session marker file deleted successfully: " + g_SessionPath);
}