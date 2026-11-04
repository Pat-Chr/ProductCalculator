#include "framework.h"
#include "LanguageStore.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	std::filesystem::path FindLanguageFilePath(const std::wstring& languageCode)
	{
		wchar_t modulePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

		std::filesystem::path currentPath = std::filesystem::path(modulePath).parent_path();
		const std::filesystem::path relativePath = std::filesystem::path(L"languages") / (languageCode + L".json");
		while (!currentPath.empty())
		{
			const std::filesystem::path candidatePath = currentPath / relativePath;
			std::error_code ec;
			if (std::filesystem::exists(candidatePath, ec) && std::filesystem::is_regular_file(candidatePath, ec))
			{
				return candidatePath;
			}

			if (currentPath == currentPath.root_path())
			{
				break;
			}

			currentPath = currentPath.parent_path();
		}

		return {};
	}

	std::wstring ToWideString(const std::string& text)
	{
		if (text.empty())
		{
			return L"";
		}

		int requiredSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
		UINT codePage = CP_UTF8;
		DWORD flags = MB_ERR_INVALID_CHARS;

		if (requiredSize == 0)
		{
			codePage = CP_ACP;
			flags = 0;
			requiredSize = MultiByteToWideChar(codePage, flags, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
		}

		if (requiredSize == 0)
		{
			return L"";
		}

		std::wstring result(requiredSize, L'\0');
		MultiByteToWideChar(codePage, flags, text.c_str(), static_cast<int>(text.size()), result.data(), requiredSize);
		return result;
	}

	void SkipWhitespace(const std::string& text, size_t& index)
	{
		while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0)
		{
			++index;
		}
	}

	bool ParseJsonStringToken(const std::string& text, size_t& index, std::string& parsedValue)
	{
		if (index >= text.size() || text[index] != '"')
		{
			return false;
		}

		++index;
		parsedValue.clear();

		while (index < text.size())
		{
			const char ch = text[index++];
			if (ch == '"')
			{
				return true;
			}

			if (ch != '\\')
			{
				parsedValue.push_back(ch);
				continue;
			}

			if (index >= text.size())
			{
				return false;
			}

			const char escaped = text[index++];
			switch (escaped)
			{
			case '"':
			case '\\':
			case '/':
				parsedValue.push_back(escaped);
				break;
			case 'b':
				parsedValue.push_back('\b');
				break;
			case 'f':
				parsedValue.push_back('\f');
				break;
			case 'n':
				parsedValue.push_back('\n');
				break;
			case 'r':
				parsedValue.push_back('\r');
				break;
			case 't':
				parsedValue.push_back('\t');
				break;
			case 'u':
				if (index + 4 <= text.size())
				{
					index += 4;
					parsedValue.push_back('?');
				}
				else
				{
					return false;
				}
				break;
			default:
				return false;
			}
		}

		return false;
	}

	bool ParseLanguageJson(const std::string& content, std::map<std::wstring, std::wstring>& resources)
	{
		resources.clear();

		size_t index = 0;
		SkipWhitespace(content, index);
		if (index >= content.size() || content[index] != '{')
		{
			return false;
		}

		++index;
		while (index < content.size())
		{
			SkipWhitespace(content, index);
			if (index >= content.size())
			{
				return false;
			}

			if (content[index] == '}')
			{
				++index;
				return true;
			}

			std::string key;
			if (!ParseJsonStringToken(content, index, key))
			{
				return false;
			}

			SkipWhitespace(content, index);
			if (index >= content.size() || content[index] != ':')
			{
				return false;
			}

			++index;
			SkipWhitespace(content, index);

			std::string value;
			if (!ParseJsonStringToken(content, index, value))
			{
				return false;
			}

			resources[ToWideString(key)] = ToWideString(value);

			SkipWhitespace(content, index);
			if (index >= content.size())
			{
				return false;
			}

			if (content[index] == ',')
			{
				++index;
				continue;
			}

			if (content[index] == '}')
			{
				++index;
				return true;
			}

			return false;
		}

		return false;
	}
}

bool LoadLanguageResources(const std::wstring& languageCode, std::map<std::wstring, std::wstring>& resources)
{
	const std::filesystem::path languagePath = FindLanguageFilePath(languageCode);
	if (languagePath.empty())
	{
		return false;
	}

	std::ifstream file(languagePath, std::ios::binary);
	if (!file)
	{
		return false;
	}

	const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	return ParseLanguageJson(content, resources);
}

std::wstring GetLocalizedText(const std::map<std::wstring, std::wstring>& resources, const std::wstring& key, const std::wstring& fallback)
{
	const auto it = resources.find(key);
	if (it == resources.end() || it->second.empty())
	{
		return fallback;
	}

	return it->second;
}
