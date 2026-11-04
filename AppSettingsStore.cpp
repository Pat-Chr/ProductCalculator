#include "framework.h"
#include "AppSettingsStore.h"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace
{
	std::filesystem::path FindSettingsPathInCurrentOrParents(const std::wstring& fileName)
	{
		wchar_t modulePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

		std::filesystem::path currentPath = std::filesystem::path(modulePath).parent_path();
		while (!currentPath.empty())
		{
			const std::filesystem::path candidatePath = currentPath / fileName;
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

	std::filesystem::path GetSettingsPathForWrite(const std::wstring& fileName)
	{
		const std::filesystem::path existingPath = FindSettingsPathInCurrentOrParents(fileName);
		if (!existingPath.empty())
		{
			return existingPath;
		}

		wchar_t modulePath[MAX_PATH] = {};
		GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		return std::filesystem::path(modulePath).parent_path() / fileName;
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

	std::string ToUtf8String(const std::wstring& text)
	{
		if (text.empty())
		{
			return "";
		}

		const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (requiredSize <= 0)
		{
			return "";
		}

		std::string result(static_cast<size_t>(requiredSize), '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), requiredSize, nullptr, nullptr);
		return result;
	}

	void SkipWhitespace(const std::string& text, size_t& index)
	{
		while (index < text.size() && std::isspace(static_cast<unsigned char>(text[index])) != 0)
		{
			++index;
		}
	}

	std::string Trim(const std::string& text)
	{
		size_t start = 0;
		size_t end = text.size();

		while (start < end && std::isspace(static_cast<unsigned char>(text[start])) != 0)
		{
			++start;
		}

		while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
		{
			--end;
		}

		return text.substr(start, end - start);
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

	bool ConsumeJsonValue(const std::string& text, size_t& index, std::string& rawValue)
	{
		SkipWhitespace(text, index);
		if (index >= text.size())
		{
			return false;
		}

		const size_t valueStart = index;
		const char first = text[index];

		if (first == '"')
		{
			std::string ignored;
			if (!ParseJsonStringToken(text, index, ignored))
			{
				return false;
			}

			rawValue = text.substr(valueStart, index - valueStart);
			return true;
		}

		if (first == '{' || first == '[')
		{
			const char open = first;
			const char close = (first == '{') ? '}' : ']';
			int depth = 0;
			bool inString = false;

			while (index < text.size())
			{
				const char ch = text[index++];

				if (inString)
				{
					if (ch == '\\')
					{
						if (index < text.size())
						{
							++index;
						}
					}
					else if (ch == '"')
					{
						inString = false;
					}
					continue;
				}

				if (ch == '"')
				{
					inString = true;
					continue;
				}

				if (ch == open)
				{
					++depth;
				}
				else if (ch == close)
				{
					--depth;
					if (depth == 0)
					{
						rawValue = text.substr(valueStart, index - valueStart);
						return true;
					}
				}
			}

			return false;
		}

		while (index < text.size() && text[index] != ',' && text[index] != '}')
		{
			++index;
		}

		rawValue = Trim(text.substr(valueStart, index - valueStart));
		return !rawValue.empty();
	}

	bool ParseTopLevelJsonEntries(const std::string& content, std::map<std::wstring, std::wstring>& entries)
	{
		entries.clear();

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

			std::string rawValue;
			if (!ConsumeJsonValue(content, index, rawValue))
			{
				return false;
			}

			entries[ToWideString(key)] = ToWideString(rawValue);

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

	bool TryParseJsonStringLiteral(const std::wstring& rawJsonValue, std::wstring& parsedValue)
	{
		std::string utf8RawValue = ToUtf8String(rawJsonValue);
		size_t index = 0;
		std::string parsedUtf8Value;
		if (!ParseJsonStringToken(utf8RawValue, index, parsedUtf8Value))
		{
			return false;
		}

		parsedValue = ToWideString(parsedUtf8Value);
		return true;
	}

	std::string EscapeJsonString(const std::string& value)
	{
		std::string escaped;
		escaped.reserve(value.size() + 8);

		for (const unsigned char ch : value)
		{
			switch (ch)
			{
			case '"':
				escaped += "\\\"";
				break;
			case '\\':
				escaped += "\\\\";
				break;
			case '\b':
				escaped += "\\b";
				break;
			case '\f':
				escaped += "\\f";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				if (ch < 0x20)
				{
					std::ostringstream out;
					out << "\\u00";
					const char hex[] = "0123456789ABCDEF";
					out << hex[(ch >> 4) & 0x0F] << hex[ch & 0x0F];
					escaped += out.str();
				}
				else
				{
					escaped.push_back(static_cast<char>(ch));
				}
				break;
			}
		}

		return escaped;
	}

	bool WriteSettingsFile(const std::filesystem::path& filePath, const AppSettings& settings)
	{
		std::map<std::wstring, std::wstring> values = settings.additionalRawJsonValues;

		const std::string currencyUtf8 = ToUtf8String(settings.standardwaehrung);
		values[L"standardwaehrung"] = ToWideString(std::string("\"") + EscapeJsonString(currencyUtf8) + "\"");

		const std::string languageUtf8 = ToUtf8String(settings.sprache);
		values[L"sprache"] = ToWideString(std::string("\"") + EscapeJsonString(languageUtf8) + "\"");

		std::string output = "{\n";
		bool first = true;
		for (const auto& entry : values)
		{
			if (!first)
			{
				output += ",\n";
			}
			first = false;

			const std::string keyUtf8 = ToUtf8String(entry.first);
			const std::string valueUtf8 = ToUtf8String(entry.second);

			output += "  \"" + EscapeJsonString(keyUtf8) + "\": " + valueUtf8;
		}
		output += "\n}\n";

		std::ofstream file(filePath, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}

		file.write(output.data(), static_cast<std::streamsize>(output.size()));
		return static_cast<bool>(file);
	}

	}

bool LoadAppSettings(AppSettings& settings)
{
	settings = AppSettings{};

	const std::filesystem::path settingsPath = FindSettingsPathInCurrentOrParents(L"settings.json");
	if (settingsPath.empty())
	{
		return false;
	}

	std::ifstream file(settingsPath, std::ios::binary);
	if (!file)
	{
		return false;
	}

	const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	std::map<std::wstring, std::wstring> entries;
	if (!ParseTopLevelJsonEntries(content, entries))
	{
		return false;
	}

	for (const auto& entry : entries)
	{
		if (_wcsicmp(entry.first.c_str(), L"standardwaehrung") == 0)
		{
			std::wstring parsedCurrency;
			if (TryParseJsonStringLiteral(entry.second, parsedCurrency) && !parsedCurrency.empty())
			{
				settings.standardwaehrung = parsedCurrency;
			}
			continue;
		}

		if (_wcsicmp(entry.first.c_str(), L"sprache") == 0)
		{
			std::wstring parsedLanguage;
			if (TryParseJsonStringLiteral(entry.second, parsedLanguage) && !parsedLanguage.empty())
			{
				settings.sprache = parsedLanguage;
			}
			continue;
		}

		settings.additionalRawJsonValues[entry.first] = entry.second;
	}

	return true;
}

bool SaveAppSettings(const AppSettings& settings)
{
	const std::filesystem::path settingsPath = GetSettingsPathForWrite(L"settings.json");
	if (settingsPath.empty())
	{
		return false;
	}

	return WriteSettingsFile(settingsPath, settings);
}

