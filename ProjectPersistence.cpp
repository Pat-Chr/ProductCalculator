#include "framework.h"
#include "ProjectPersistence.h"

#include <algorithm>
#include <array>
#include <ctime>
#include <fstream>
#include <sstream>
#include <commdlg.h>

namespace
{
	std::wstring SanitizeFileName(std::wstring name)
	{
		const std::wstring invalidChars = L"\\/:*?\"<>|";
		for (wchar_t& ch : name)
		{
			if (invalidChars.find(ch) != std::wstring::npos)
			{
				ch = L'_';
			}
		}

		if (name.empty())
		{
			name = L"projekt";
		}

		return name;
	}

	std::wstring BuildDefaultFileName(const SavedProjectData& data)
	{
		std::wstring baseName = L"projekt";
		if (!data.productName.empty())
		{
			baseName = data.productName;
		}
		else if (!data.productPath.empty())
		{
			baseName = std::filesystem::path(data.productPath).filename().wstring();
		}

		const std::time_t now = std::time(nullptr);
		std::tm tmNow{};
		localtime_s(&tmNow, &now);

		wchar_t stamp[32] = {};
		wcsftime(stamp, _countof(stamp), L"%Y%m%d_%H%M%S", &tmNow);

		return SanitizeFileName(baseName) + L"_" + stamp + L".pcproj";
	}

	std::wstring JoinQuantities(const std::vector<int>& quantities)
	{
		std::wostringstream out;
		for (size_t i = 0; i < quantities.size(); ++i)
		{
			if (i > 0)
			{
				out << L",";
			}
			out << max(0, quantities[i]);
		}
		return out.str();
	}

	std::vector<int> ParseQuantities(const std::wstring& value)
	{
		std::vector<int> quantities;
		std::wstringstream stream(value);
		std::wstring token;

		while (std::getline(stream, token, L','))
		{
			if (token.empty())
			{
				quantities.push_back(0);
				continue;
			}

			try
			{
				const int quantity = std::stoi(token);
				quantities.push_back(max(0, quantity));
			}
			catch (...)
			{
				quantities.push_back(0);
			}
		}

		return quantities;
	}

	std::wstring ExtractProductName(const std::wstring& productPath)
	{
		if (productPath.empty())
		{
			return L"";
		}

		const std::wstring fileName = std::filesystem::path(productPath).filename().wstring();
		if (!fileName.empty())
		{
			return fileName;
		}

		return productPath;
	}

	std::wstring EncodeUtf8AsHex(const std::string& value)
	{
		static const wchar_t hexDigits[] = L"0123456789ABCDEF";
		std::wstring encoded;
		encoded.reserve(value.size() * 2);

		for (unsigned char byte : value)
		{
			encoded.push_back(hexDigits[(byte >> 4) & 0x0F]);
			encoded.push_back(hexDigits[byte & 0x0F]);
		}

		return encoded;
	}

	int ParseHexNibble(wchar_t ch)
	{
		if (ch >= L'0' && ch <= L'9')
		{
			return static_cast<int>(ch - L'0');
		}
		if (ch >= L'a' && ch <= L'f')
		{
			return static_cast<int>(10 + (ch - L'a'));
		}
		if (ch >= L'A' && ch <= L'F')
		{
			return static_cast<int>(10 + (ch - L'A'));
		}
		return -1;
	}

	bool DecodeHexToUtf8(const std::wstring& value, std::string& decoded)
	{
		if ((value.size() % 2) != 0)
		{
			return false;
		}

		decoded.clear();
		decoded.reserve(value.size() / 2);

		for (size_t i = 0; i < value.size(); i += 2)
		{
			const int high = ParseHexNibble(value[i]);
			const int low = ParseHexNibble(value[i + 1]);
			if (high < 0 || low < 0)
			{
				return false;
			}

			const char byteValue = static_cast<char>((high << 4) | low);
			decoded.push_back(byteValue);
		}

		return true;
	}

	bool SaveProjectDataToFile(const std::filesystem::path& filePath, const SavedProjectData& data)
	{
		std::wofstream file(filePath, std::ios::trunc);
		if (!file)
		{
			return false;
		}

		file << L"productPath=" << data.productPath << L"\n";
		file << L"productName=" << data.productName << L"\n";
		file << L"productJsonHex=" << EncodeUtf8AsHex(data.productJsonUtf8) << L"\n";
		file << L"quantities=" << JoinQuantities(data.optionQuantities) << L"\n";
		return static_cast<bool>(file);
	}

	bool LoadProjectDataFromFile(const std::filesystem::path& filePath, SavedProjectData& data)
	{
		std::wifstream file(filePath);
		if (!file)
		{
			return false;
		}

		SavedProjectData loadedData;
		std::wstring line;

		while (std::getline(file, line))
		{
			const size_t separator = line.find(L'=');
			if (separator == std::wstring::npos)
			{
				continue;
			}

			const std::wstring key = line.substr(0, separator);
			const std::wstring value = line.substr(separator + 1);

			if (key == L"productPath")
			{
				loadedData.productPath = value;
			}
			else if (key == L"productName")
			{
				loadedData.productName = value;
			}
			else if (key == L"productJsonHex")
			{
				DecodeHexToUtf8(value, loadedData.productJsonUtf8);
			}
			else if (key == L"quantities")
			{
				loadedData.optionQuantities = ParseQuantities(value);
			}
		}

		if (loadedData.productPath.empty())
		{
			return false;
		}

		if (loadedData.productName.empty())
		{
			loadedData.productName = ExtractProductName(loadedData.productPath);
		}

		data = std::move(loadedData);
		return true;
	}
}

std::filesystem::path EnsureSavedProjectsDirectory()
{
	wchar_t modulePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

	const std::filesystem::path outputDirectory = std::filesystem::path(modulePath).parent_path() / L"saved_projects";
	std::error_code ec;
	std::filesystem::create_directories(outputDirectory, ec);
	return outputDirectory;
}

bool SaveProjectDataWithDialog(HWND hOwner, const SavedProjectData& data, std::wstring& savedFilePath)
{
	if (data.productPath.empty())
	{
		return false;
	}

	const std::filesystem::path savedProjectsDirectory = EnsureSavedProjectsDirectory();

	std::array<wchar_t, MAX_PATH> fileNameBuffer = {};
	const std::wstring defaultFileName = BuildDefaultFileName(data);
	wcsncpy_s(fileNameBuffer.data(), fileNameBuffer.size(), defaultFileName.c_str(), _TRUNCATE);

	std::array<wchar_t, MAX_PATH> initialDirectory = {};
	const std::wstring initialDirectoryPath = savedProjectsDirectory.wstring();
	wcsncpy_s(initialDirectory.data(), initialDirectory.size(), initialDirectoryPath.c_str(), _TRUNCATE);

	wchar_t filter[] = L"Projektdateien (*.pcproj)\0*.pcproj\0Alle Dateien (*.*)\0*.*\0\0";

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hOwner;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = fileNameBuffer.data();
	ofn.nMaxFile = static_cast<DWORD>(fileNameBuffer.size());
	ofn.lpstrInitialDir = initialDirectory.data();
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = L"pcproj";

	if (!GetSaveFileNameW(&ofn))
	{
		return false;
	}

	const std::filesystem::path outputPath = fileNameBuffer.data();
	SavedProjectData dataToSave = data;
	if (dataToSave.productName.empty())
	{
		dataToSave.productName = ExtractProductName(dataToSave.productPath);
	}

	if (!SaveProjectDataToFile(outputPath, dataToSave))
	{
		return false;
	}

	savedFilePath = outputPath.wstring();
	return true;
}

bool LoadProjectDataWithDialog(HWND hOwner, SavedProjectData& data, std::wstring& loadedFilePath)
{
	const std::filesystem::path savedProjectsDirectory = EnsureSavedProjectsDirectory();

	std::array<wchar_t, MAX_PATH> fileNameBuffer = {};
	std::array<wchar_t, MAX_PATH> initialDirectory = {};
	const std::wstring initialDirectoryPath = savedProjectsDirectory.wstring();
	wcsncpy_s(initialDirectory.data(), initialDirectory.size(), initialDirectoryPath.c_str(), _TRUNCATE);

	wchar_t filter[] = L"Projektdateien (*.pcproj)\0*.pcproj\0Alle Dateien (*.*)\0*.*\0\0";

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = hOwner;
	ofn.lpstrFilter = filter;
	ofn.lpstrFile = fileNameBuffer.data();
	ofn.nMaxFile = static_cast<DWORD>(fileNameBuffer.size());
	ofn.lpstrInitialDir = initialDirectory.data();
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	ofn.lpstrDefExt = L"pcproj";

	if (!GetOpenFileNameW(&ofn))
	{
		return false;
	}

	const std::filesystem::path inputPath = fileNameBuffer.data();
	if (!LoadProjectDataFromFile(inputPath, data))
	{
		return false;
	}

	loadedFilePath = inputPath.wstring();
	return true;
}
