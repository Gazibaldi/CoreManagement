#pragma once
#include <string>

extern std::string g_SessionPath;

void PrepareNewSessionLogFile();
void CreateSessionMarkerFile();
void DeleteSessionMarkerFile();