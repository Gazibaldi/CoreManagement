#pragma once
#include <string>

extern std::string g_SessionPath;

void PrepareNewSessionLogFile();
void TryCreateSessionMarkerFile();
void DeleteSessionMarkerFile();