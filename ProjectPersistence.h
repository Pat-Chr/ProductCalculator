#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <windows.h>

struct SavedProjectData
{
	std::wstring productPath;
	std::wstring productName;
	std::string productJsonUtf8;
	std::vector<int> optionQuantities;
};

std::filesystem::path EnsureSavedProjectsDirectory();
bool SaveProjectDataWithDialog(HWND hOwner, const SavedProjectData& data, std::wstring& savedFilePath);
bool LoadProjectDataWithDialog(HWND hOwner, SavedProjectData& data, std::wstring& loadedFilePath);
