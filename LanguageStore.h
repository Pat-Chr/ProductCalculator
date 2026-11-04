#pragma once

#include <map>
#include <string>

bool LoadLanguageResources(const std::wstring& languageCode, std::map<std::wstring, std::wstring>& resources);
std::wstring GetLocalizedText(const std::map<std::wstring, std::wstring>& resources, const std::wstring& key, const std::wstring& fallback);
