#pragma once

#include "Resource.h"

#include <cstddef>
#include <string>
#include <vector>
#include <windows.h>

bool SaveCurrentProject(HWND hOwner);
bool LoadProjectIntoCalculator(HWND hMainWindow, HWND hDialogOwner);

struct ProductOption
{
    std::wstring name;
    std::wstring articleNumber = L"-";
    double price = 0.0;
};

struct ProductConfiguration
{
    std::wstring articleNumber = L"-";
    double basePrice = 0.0;
    std::vector<ProductOption> options;
};

struct AppSettings
{
    std::wstring standardwaehrung = L"CHF";
};

extern HINSTANCE hInst;
extern HWND g_hProductDropdown;
extern std::vector<HWND> g_optionQuantityInputs;
extern std::wstring g_selectedProductPath;
extern ProductConfiguration g_currentConfiguration;

std::wstring FormatPrice(double value);
std::wstring GetCurrentCurrency();
int GetOptionQuantity(size_t index);
void ShowSummaryWindow(HWND hParent);
