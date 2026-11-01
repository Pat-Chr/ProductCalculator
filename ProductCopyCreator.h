#pragma once

#include <filesystem>
#include <windows.h>

enum class ProductCopyStatus
{
	Copied,
	Cancelled,
	Failed
};

ProductCopyStatus CopyExistingProductWithNewName(HWND ownerWindow, const std::filesystem::path& sourceProductPath, std::filesystem::path& copiedProductPath);
