#pragma once

#include <string>
#include <windows.h>

enum class ProductDeletionStatus
{
	Cancelled,
	Deleted,
	Failed
};

ProductDeletionStatus ConfirmAndDeleteProduct(HWND ownerWindow, const std::wstring& productFolderPath);
