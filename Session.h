#pragma once
#include <string>

extern std::string g_SessionPath;

void PrepareNewSession();
void CreateSessionMarkerFile();
void DeleteSessionMarkerFile();