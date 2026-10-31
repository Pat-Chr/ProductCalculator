#pragma once

#include <string>
#include <windows.h>

constexpr UINT WM_PRODUCT_CONFIGURATION_UPDATED = WM_APP + 1;

bool OpenEditProductWindow(HWND ownerWindow, const std::wstring& productFolderPath);
