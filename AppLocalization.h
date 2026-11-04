#pragma once

#include "ProductCalculator.h"
#include "LanguageStore.h"

#include <algorithm>
#include <cwctype>
#include <map>
#include <string>

namespace AppLocalization
{
	inline std::wstring g_loadedLanguageCode;
	inline std::map<std::wstring, std::wstring> g_resources;

	inline std::wstring TrimWhitespace(const std::wstring& value)
	{
		const size_t first = value.find_first_not_of(L" \t\r\n");
		if (first == std::wstring::npos)
		{
			return L"";
		}

		const size_t last = value.find_last_not_of(L" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	inline std::wstring NormalizeLanguageCode(const std::wstring& languageCode)
	{
		std::wstring normalized = TrimWhitespace(languageCode);
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });
		if (normalized == L"en")
		{
			return L"en";
		}
		return L"de";
	}

	inline void SetLanguageCode(const std::wstring& languageCode)
	{
		const std::wstring normalizedLanguageCode = NormalizeLanguageCode(languageCode);
		if (g_loadedLanguageCode == normalizedLanguageCode && !g_resources.empty())
		{
			return;
		}

		if (!LoadLanguageResources(normalizedLanguageCode, g_resources))
		{
			LoadLanguageResources(L"de", g_resources);
			g_loadedLanguageCode = L"de";
			return;
		}

		g_loadedLanguageCode = normalizedLanguageCode;
	}

	inline void SetLanguageFromSettings(const AppSettings& settings)
	{
		SetLanguageCode(settings.sprache);
	}

	inline std::wstring Text(const std::wstring& key, const std::wstring& fallback)
	{
		if (g_resources.empty())
		{
			SetLanguageCode(L"de");
		}
		return GetLocalizedText(g_resources, key, fallback);
	}
}
