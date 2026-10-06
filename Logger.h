#pragma once
#include <string>
#include <atomic>

enum class LogLevel : int { Disabled = 0, Standard = 1, Verbose = 2 };

extern std::atomic<bool> g_RunLogThread;
extern std::string g_LogPath;

void StartAsyncLogger();
void StopAsyncLogger();
void WriteLog(LogLevel requiredLevel, const std::string& message);
