#pragma once
#include <string>
namespace StringUtility {
    std::wstring ConvertString(const std::string& text);
    std::string ConvertString(const std::wstring& text);
}
