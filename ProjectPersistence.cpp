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
		if (!data.productPath.empty())
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

	bool CreateSnapshotForSavedProject(const std::filesystem::path& projectFilePath, const SavedProjectData& sourceData, SavedProjectData& snapshotData)
	{
		const std::filesystem::path sourceProductPath(sourceData.productPath);
		std::error_code ec;
		if (!std::filesystem::exists(sourceProductPath, ec) || !std::filesystem::is_directory(sourceProductPath, ec))
		{
			return false;
		}

		const std::filesystem::path snapshotDirectory = projectFilePath.parent_path() / SanitizeFileName(projectFilePath.stem().wstring());
		std::filesystem::remove_all(snapshotDirectory, ec);
		ec.clear();
		if (!std::filesystem::create_directories(snapshotDirectory, ec) || ec)
		{
			return false;
		}

		std::wstring sourceFolderName = sourceProductPath.filename().wstring();
		if (sourceFolderName.empty())
		{
			sourceFolderName = L"produkt";
		}

		const std::filesystem::path snapshotProductPath = snapshotDirectory / SanitizeFileName(sourceFolderName);
		std::filesystem::copy(sourceProductPath, snapshotProductPath, std::filesystem::copy_options::recursive, ec);
		if (ec)
		{
			return false;
		}

		snapshotData = sourceData;
		snapshotData.productPath = snapshotProductPath.wstring();
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
			else if (key == L"quantities")
			{
				loadedData.optionQuantities = ParseQuantities(value);
			}
		}

		if (loadedData.productPath.empty())
		{
			return false;
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
	SavedProjectData snapshotData;
	if (!CreateSnapshotForSavedProject(outputPath, data, snapshotData))
	{
		return false;
	}

	if (!SaveProjectDataToFile(outputPath, snapshotData))
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
