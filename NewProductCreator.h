#pragma once

#include <filesystem>
#include <windows.h>

enum class NewProductCreationStatus
{
	Created,
	Cancelled,
	Failed
};

NewProductCreationStatus CreateNewProductFolderWithDefaults(HWND ownerWindow, std::filesystem::path& createdProductPath);
