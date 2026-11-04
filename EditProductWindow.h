#pragma once

#include <string>
#include <windows.h>

// Event constant for product configuration updates.
// Sent from the edit window to the main window to trigger synchronization.
constexpr UINT WM_PRODUCT_CONFIGURATION_UPDATED = WM_APP + 1;

// Opens the edit window for product editing.
// If the window is already open, it is only focused and restored.
// Parameters:
//   ownerWindow - The main window (for modality and messages)
//   productFolderPath - Path to the product data directory containing product.json
//   showProjectSaveButton - Controls whether the "Projekt speichern" button is visible
//   showProductSaveButton - Controls whether the "Produkt speichern" button is visible
// Return value:
//   true - If the window was opened successfully (or was already open)
//   false - If ownerWindow is nullptr or productFolderPath is empty
bool OpenEditProductWindow(HWND ownerWindow, const std::wstring& productFolderPath, bool showProjectSaveButton = true, bool showProductSaveButton = true);
