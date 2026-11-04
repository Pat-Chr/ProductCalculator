#pragma once

#include "Resource.h"

#include <cstddef>
#include <map>
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
    std::wstring sprache = L"de";
    std::map<std::wstring, std::wstring> additionalRawJsonValues;
};

extern HINSTANCE hInst;
extern HWND g_hProductDropdown;
extern std::vector<HWND> g_optionQuantityInputs;
extern std::wstring g_selectedProductPath;
extern ProductConfiguration g_currentConfiguration;
extern AppSettings g_appSettings;

std::wstring FormatPrice(double value);
std::wstring GetCurrentCurrency();
int GetOptionQuantity(size_t index);
void ShowSummaryWindow(HWND hParent);
