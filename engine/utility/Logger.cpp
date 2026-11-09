#include "Logger.h"
#include <Windows.h>
void Logger::Log(const std::string& message) { OutputDebugStringA(message.c_str()); }
void Logger::Log(const std::wstring& message) { OutputDebugStringW(message.c_str()); }
