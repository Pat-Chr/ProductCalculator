#include "framework.h"
#include "ProductCalculator.h"
#include "EditProductWindow.h"
#include "NewProductCreator.h"
#include "ProductDeletion.h"
#include "ProjectPersistence.h"

#include <algorithm>
#include <fstream>
#include <filesystem>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <memory>

#define MAX_LOADSTRING 100

HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HWND g_hProductDropdown = nullptr;
HWND g_hEditProductButton = nullptr;
HWND g_hCreateProductButton = nullptr;
HWND g_hDeleteProductButton = nullptr;
HWND g_hCalcGroupBox = nullptr;
HWND g_hBasePriceLabel = nullptr;
HWND g_hQuantityHeaderLabel = nullptr;
HWND g_hTotalPriceLabel = nullptr;
HWND g_hShowSummaryButton = nullptr;
HWND g_hLoadProjectButton = nullptr;
std::vector<HWND> g_optionLabels;
std::vector<HWND> g_optionQuantityInputs;
std::vector<std::wstring> g_productFolderPaths;
std::wstring g_selectedProductPath;
ProductConfiguration g_currentConfiguration;
AppSettings g_appSettings;

ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
std::wstring ToWideString(const std::string& text);
std::filesystem::path FindFileInCurrentOrParents(const std::wstring& fileName);
bool TryLoadAppSettings(AppSettings& settings);
std::wstring FormatPrice(double value);
bool TryLoadProductConfiguration(const std::wstring& productFolderPath, ProductConfiguration& configuration);
void ClearDynamicOptionControls();
void UpdateTotalPriceLabel();
void RenderProductConfiguration(HWND hWnd);
void UpdateLayout(HWND hWnd, int clientWidth, int clientHeight);
int GetOptionQuantity(size_t index);
bool SelectProductByPath(HWND hCombo, const std::wstring& productFolderPath);

// Sucht den produktdaten-Ordner rekursiv vom EXE-Verzeichnis nach oben,
// damit zukünftige Paketierungsvarianten ohne feste Pfade funktionieren.
std::filesystem::path FindProductDataDirectory()
{
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::filesystem::path currentPath = std::filesystem::path(modulePath).parent_path();
    while (!currentPath.empty())
    {
        const std::filesystem::path candidatePath = currentPath / L"produktdaten";
        std::error_code ec;
        if (std::filesystem::exists(candidatePath, ec) && std::filesystem::is_directory(candidatePath, ec))
        {
            return candidatePath;
        }

        if (currentPath == currentPath.root_path())
        {
            break;
        }

        currentPath = currentPath.parent_path();
    }

    return {};
}

bool SelectProductByPath(HWND hCombo, const std::wstring& productFolderPath)
{
    if (hCombo == nullptr || productFolderPath.empty())
    {
        return false;
    }

    for (size_t i = 0; i < g_productFolderPaths.size(); ++i)
    {
        if (g_productFolderPaths[i] == productFolderPath)
        {
            SendMessageW(hCombo, CB_SETCURSEL, static_cast<WPARAM>(i), 0);
            g_selectedProductPath = productFolderPath;
            return true;
        }
    }

    return false;
}

// Sucht eine Datei rekursiv vom EXE-Verzeichnis nach oben,
// um dynamische Konfigurationsdateien zu finden.
std::filesystem::path FindFileInCurrentOrParents(const std::wstring& fileName)
{
    wchar_t modulePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

    std::filesystem::path currentPath = std::filesystem::path(modulePath).parent_path();
    while (!currentPath.empty())
    {
        const std::filesystem::path candidatePath = currentPath / fileName;
        std::error_code ec;
        if (std::filesystem::exists(candidatePath, ec) && std::filesystem::is_regular_file(candidatePath, ec))
        {
            return candidatePath;
        }

        if (currentPath == currentPath.root_path())
        {
            break;
        }

        currentPath = currentPath.parent_path();
    }

    return {};
}

// Konvertiert einen UTF-8 String in einen Unicode-WideString.
// Fallback auf den lokalen Codepage, wenn UTF-8 nicht funktioniert.
std::wstring ToWideString(const std::string& text)
{
    if (text.empty())
    {
        return L"";
    }

    int requiredSize = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    UINT codePage = CP_UTF8;
    DWORD flags = MB_ERR_INVALID_CHARS;

    if (requiredSize == 0)
    {
        codePage = CP_ACP;
        flags = 0;
        requiredSize = MultiByteToWideChar(codePage, flags, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    }

    if (requiredSize == 0)
    {
        return L"";
    }

    std::wstring result(requiredSize, L'\0');
    MultiByteToWideChar(codePage, flags, text.c_str(), static_cast<int>(text.size()), result.data(), requiredSize);
    return result;
}

// Lädt die Anwendungs-Einstellungen aus settings.json.
// Setzt Standardwerte zurück und extrahiert die standardwaehrung aus der JSON-Datei.
// Gibt true zurück, wenn die Datei erfolgreich gelesen wurde.
bool TryLoadAppSettings(AppSettings& settings)
{
    settings = AppSettings{};

    const std::filesystem::path settingsPath = FindFileInCurrentOrParents(L"settings.json");
    if (settingsPath.empty())
    {
        return false;
    }

    std::ifstream file(settingsPath, std::ios::binary);
    if (!file)
    {
        return false;
    }

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try
    {
        std::smatch currencyMatch;
        const std::regex currencyRegex("\"standardwaehrung\"\\s*:\\s*\"([^\"]+)\"");
        if (std::regex_search(content, currencyMatch, currencyRegex))
        {
            const std::wstring loadedCurrency = ToWideString(currencyMatch[1].str());
            if (!loadedCurrency.empty())
            {
                settings.standardwaehrung = loadedCurrency;
            }
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

// Formatiert einen Preis mit zwei Dezimalstellen und fügt die Währung hinzu.
// Beispiel: "123,45 EUR"
std::wstring FormatPrice(double value)
{
    std::wostringstream stream;
    stream.setf(std::ios::fixed);
    stream.precision(2);
    stream << value;

    std::wstring number = stream.str();
    std::replace(number.begin(), number.end(), L'.', L',');
    return number + L" " + g_appSettings.standardwaehrung;
}

// Lädt die Produktkonfiguration aus produkt.json.
// Extrahiert basispreis und optionen mittels Regex.
// Gibt false zurück, wenn die Datei nicht gefunden oder ungültig ist.
bool TryLoadProductConfiguration(const std::wstring& productFolderPath, ProductConfiguration& configuration)
{
    configuration = ProductConfiguration{};

    const std::filesystem::path configPath = std::filesystem::path(productFolderPath) / L"produkt.json";
    std::ifstream file(configPath, std::ios::binary);
    if (!file)
    {
        return false;
    }

    const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    try
    {
        std::smatch basePriceMatch;
        const std::regex basePriceRegex(R"("basispreis"\s*:\s*(-?\d+(?:\.\d+)?))");
        if (std::regex_search(content, basePriceMatch, basePriceRegex))
        {
            configuration.basePrice = std::stod(basePriceMatch[1].str());
        }

        const std::regex optionRegex(R"opt(\{\s*"name"\s*:\s*"([^"\\]*(?:\\.[^"\\]*)*)"\s*,\s*"preis"\s*:\s*(-?\d+(?:\.\d+)?)\s*\})opt");
        for (std::sregex_iterator it(content.begin(), content.end(), optionRegex); it != std::sregex_iterator(); ++it)
        {
            const std::smatch& match = *it;

            ProductOption option;
            option.name = ToWideString(match[1].str());
            option.price = std::stod(match[2].str());
            configuration.options.push_back(std::move(option));
        }
    }
    catch (...)
    {
        return false;
    }

    return true;
}

// Entfernt alle dynamisch erstellten Option-Controls (Labels und Quantity-Inputs).
// Wird aufgerufen, wenn die Produktkonfiguration sich ändert.
void ClearDynamicOptionControls()
{
    for (HWND label : g_optionLabels)
    {
        if (label != nullptr)
        {
            DestroyWindow(label);
        }
    }

    for (HWND quantityInput : g_optionQuantityInputs)
    {
        if (quantityInput != nullptr)
        {
            DestroyWindow(quantityInput);
        }
    }

    g_optionLabels.clear();
    g_optionQuantityInputs.clear();
}

// Berechnet den Endpreis basierend auf Basispreis und gewählten Optionen.
// Liest die Mengen aus den Quantity-Inputs und multipliziert mit Option-Preisen.
void UpdateTotalPriceLabel()
{
    double totalPrice = g_currentConfiguration.basePrice;

    for (size_t index = 0; index < g_optionQuantityInputs.size() && index < g_currentConfiguration.options.size(); ++index)
    {
        int quantity = 0;

        if (g_optionQuantityInputs[index] != nullptr)
        {
            wchar_t quantityText[32] = {};
            GetWindowTextW(g_optionQuantityInputs[index], quantityText, 32);
            quantity = max(0, _wtoi(quantityText));
        }

        totalPrice += g_currentConfiguration.options[index].price * quantity;
    }

    const std::wstring totalText = L"Endpreis: " + FormatPrice(totalPrice);
    SetWindowTextW(g_hTotalPriceLabel, totalText.c_str());
}

// Rendert die Produktkonfiguration im Fenster.
// Erstellt Labels und Quantity-Inputs für jede Option und aktualisiert den Endpreis.
void RenderProductConfiguration(HWND hWnd)
{
    if (g_selectedProductPath.empty())
    {
        return;
    }

    ProductConfiguration configuration;
    if (!TryLoadProductConfiguration(g_selectedProductPath, configuration))
    {
        ClearDynamicOptionControls();
        g_currentConfiguration = ProductConfiguration{};
        SetWindowTextW(g_hBasePriceLabel, (L"Basispreis: " + FormatPrice(0.0)).c_str());
        SetWindowTextW(g_hTotalPriceLabel, (L"Endpreis: " + FormatPrice(0.0)).c_str());
        MessageBoxW(hWnd, L"Die Produktkonfiguration konnte nicht geladen werden.", L"Fehler", MB_ICONERROR | MB_OK);
        return;
    }

    g_currentConfiguration = std::move(configuration);
    ClearDynamicOptionControls();

    const std::wstring baseText = L"Basispreis: " + FormatPrice(g_currentConfiguration.basePrice);
    SetWindowTextW(g_hBasePriceLabel, baseText.c_str());

    // Jede Option bekommt ein eigenes Mengenfeld; Startwert bleibt bewusst 0.
    // Erweiterungen (z. B. Mindestmengen) sollten hier zentral eingreifen.
    for (size_t index = 0; index < g_currentConfiguration.options.size(); ++index)
    {
        const ProductOption& option = g_currentConfiguration.options[index];
        const std::wstring optionText = option.name + L"(+" + FormatPrice(option.price) + L")";

        const int labelWidth = 150;
        const int labelHeight = 20;
        const int quantityWidth = 70;
        const int quantityHeight = 22;

        HWND optionLabel = CreateWindowExW(
            0,
            L"STATIC",
            optionText.c_str(),
            WS_CHILD | WS_VISIBLE,
            0,
            0,
            labelWidth,
            labelHeight,
            hWnd,
            reinterpret_cast<HMENU>(IDC_STATIC),
            hInst,
            nullptr);

        HWND quantityInput = CreateWindowExW(
            0,
            L"EDIT",
            L"0",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_LEFT | ES_NUMBER,
            0,
            0,
            quantityWidth,
            quantityHeight,
            hWnd,
            reinterpret_cast<HMENU>(IDC_OPTION_QUANTITY_BASE + index),
            hInst,
            nullptr);

        g_optionLabels.push_back(optionLabel);
        g_optionQuantityInputs.push_back(quantityInput);
    }

    RECT clientRect;
    GetClientRect(hWnd, &clientRect);
    UpdateLayout(hWnd, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
    UpdateTotalPriceLabel();
}

// Schließt das Anwendungs-Fenster (aktuell nicht vollständig implementiert).
void CloseAppButton(HWND hWnd)
{
    UNREFERENCED_PARAMETER(hWnd);
}

// Passt die Position und Größe aller Fenster-Controls an, wenn sich das Client-Änder.
// Berechnet Layout basierend auf dem verfügbaren Platz im Fenster.
void UpdateLayout(HWND hWnd, int clientWidth, int clientHeight)
{
    UNREFERENCED_PARAMETER(hWnd);

    const int left = 10;
    const int width = max(180, clientWidth - 20);

    const int editButtonWidth = 150;
    const int createButtonWidth = 150;
    const int deleteButtonWidth = 150;
    const int rowSpacing = 8;
    const int dropdownWidth = max(120, width - editButtonWidth - createButtonWidth - deleteButtonWidth - (rowSpacing * 3));

    if (g_hProductDropdown != nullptr)
    {
        MoveWindow(g_hProductDropdown, left, 35, dropdownWidth, 200, TRUE);
    }

    const int editButtonLeft = left + dropdownWidth + rowSpacing;
    if (g_hEditProductButton != nullptr)
    {
        MoveWindow(g_hEditProductButton, editButtonLeft, 35, editButtonWidth, 28, TRUE);
    }

    const int createButtonLeft = editButtonLeft + editButtonWidth + rowSpacing;
    if (g_hCreateProductButton != nullptr)
    {
        MoveWindow(g_hCreateProductButton, createButtonLeft, 35, createButtonWidth, 28, TRUE);
    }

    if (g_hDeleteProductButton != nullptr)
    {
        MoveWindow(g_hDeleteProductButton, createButtonLeft + createButtonWidth + rowSpacing, 35, deleteButtonWidth, 28, TRUE);
    }

    if (g_hCalcGroupBox != nullptr)
    {
        const int groupTop = 75;
        const int groupHeight = max(120, clientHeight - groupTop - 10);
        MoveWindow(g_hCalcGroupBox, left, groupTop, width, groupHeight, TRUE);

        if (g_hBasePriceLabel != nullptr)
        {
            MoveWindow(g_hBasePriceLabel, left + 10, groupTop + 20, width - 20, 20, TRUE);
        }

        const int quantityWidth = 70;
        const int quantitySpacing = 8;
        const int checkboxWidth = max(120, width - 20 - quantityWidth - quantitySpacing);
        const int quantityLeft = left + 10 + checkboxWidth + quantitySpacing;

        if (g_hQuantityHeaderLabel != nullptr)
        {
            MoveWindow(g_hQuantityHeaderLabel, quantityLeft, groupTop + 45, quantityWidth, 18, TRUE);
        }

        int nextTop = groupTop + 66;
        for (size_t index = 0; index < g_optionLabels.size(); ++index)
        {
            if (g_optionLabels[index] != nullptr)
            {
                MoveWindow(g_optionLabels[index], left + 10, nextTop, checkboxWidth, 20, TRUE);
            }

            if (index < g_optionQuantityInputs.size() && g_optionQuantityInputs[index] != nullptr)
            {
                MoveWindow(g_optionQuantityInputs[index], quantityLeft, nextTop - 1, quantityWidth, 22, TRUE);
            }

            nextTop += 24;
        }

        if (g_hTotalPriceLabel != nullptr)
        {
            MoveWindow(g_hTotalPriceLabel, left + 10, max(groupTop + 66, nextTop + 10), width - 20, 22, TRUE);
        }
    }

    if (g_hShowSummaryButton != nullptr)
    {
        MoveWindow(g_hShowSummaryButton, 20, clientHeight - 56, 160, 28, TRUE);
    }

    if (g_hLoadProjectButton != nullptr)
    {
        MoveWindow(g_hLoadProjectButton, 188, clientHeight - 56, 160, 28, TRUE);
    }
}

// Füllt das Produkt-Dropdown mit allen verfügbaren Produktdaten-Verzeichnissen.
// Sortiert die Ordner alphabetisch und setzt den ersten als Standardauswahl.
void PopulateProductDropdown(HWND hCombo)
{
    SendMessageW(hCombo, CB_RESETCONTENT, 0, 0);
    g_productFolderPaths.clear();
    g_selectedProductPath.clear();

    const std::filesystem::path productDataPath = FindProductDataDirectory();
    if (productDataPath.empty())
    {
        SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Kein produktdaten-Ordner gefunden"));
        EnableWindow(hCombo, FALSE);
        return;
    }

    std::vector<std::filesystem::path> productFolders;
    for (const auto& entry : std::filesystem::directory_iterator(productDataPath))
    {
        std::error_code ec;
        if (entry.is_directory(ec))
        {
            productFolders.push_back(entry.path());
        }
    }

    std::sort(productFolders.begin(), productFolders.end(), [](const auto& left, const auto& right)
    {
        return left.filename().wstring() < right.filename().wstring();
    });

    for (const auto& productFolder : productFolders)
    {
        const std::wstring productName = productFolder.filename().wstring();
        const int itemIndex = static_cast<int>(SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(productName.c_str())));
        if (itemIndex >= 0)
        {
            g_productFolderPaths.push_back(productFolder.wstring());
            // ItemData speichert den Index in g_productFolderPaths, damit später
            // aus der UI-Auswahl schnell und stabil der echte Ordnerpfad geladen wird.
            SendMessageW(hCombo, CB_SETITEMDATA, static_cast<WPARAM>(itemIndex), static_cast<LPARAM>(g_productFolderPaths.size() - 1));
        }
    }

    if (g_productFolderPaths.empty())
    {
        SendMessageW(hCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"Keine Produktordner vorhanden"));
        EnableWindow(hCombo, FALSE);
        return;
    }

    EnableWindow(hCombo, TRUE);
    SendMessageW(hCombo, CB_SETCURSEL, 0, 0);
    g_selectedProductPath = g_productFolderPaths.front();
}

bool SaveCurrentProject(HWND hOwner)
{
    if (g_selectedProductPath.empty())
    {
        MessageBoxW(hOwner, L"Bitte wählen Sie zuerst ein Produkt aus.", L"Projekt speichern", MB_OK | MB_ICONINFORMATION);
        return false;
    }

    SavedProjectData data;
    data.productPath = g_selectedProductPath;
    data.optionQuantities.reserve(g_optionQuantityInputs.size());

    for (size_t index = 0; index < g_optionQuantityInputs.size(); ++index)
    {
        data.optionQuantities.push_back(GetOptionQuantity(index));
    }

    std::wstring savedFilePath;
    if (!SaveProjectDataWithDialog(hOwner, data, savedFilePath))
    {
        return false;
    }

    MessageBoxW(hOwner, L"Projekt wurde gespeichert.", L"Projekt speichern", MB_OK | MB_ICONINFORMATION);
    return true;
}

bool LoadProjectIntoCalculator(HWND hMainWindow, HWND hDialogOwner)
{
    if (hMainWindow == nullptr || g_hProductDropdown == nullptr)
    {
        return false;
    }

    SavedProjectData data;
    std::wstring loadedFilePath;
    if (!LoadProjectDataWithDialog(hDialogOwner, data, loadedFilePath))
    {
        return false;
    }

    if (!SelectProductByPath(g_hProductDropdown, data.productPath))
    {
        const std::filesystem::path loadedProductPath(data.productPath);
        const std::filesystem::path productJsonPath = loadedProductPath / L"produkt.json";
        std::error_code ec;
        const bool hasSavedSnapshot = std::filesystem::exists(productJsonPath, ec) && std::filesystem::is_regular_file(productJsonPath, ec);
        if (!hasSavedSnapshot)
        {
            MessageBoxW(hDialogOwner, L"Das gespeicherte Produkt wurde nicht gefunden.", L"Projekt laden", MB_OK | MB_ICONERROR);
            return false;
        }

        g_selectedProductPath = data.productPath;
        SendMessageW(g_hProductDropdown, CB_SETCURSEL, static_cast<WPARAM>(-1), 0);
    }

    RenderProductConfiguration(hMainWindow);

    for (size_t index = 0; index < g_optionQuantityInputs.size(); ++index)
    {
        const int quantity = (index < data.optionQuantities.size()) ? max(0, data.optionQuantities[index]) : 0;
        if (g_optionQuantityInputs[index] != nullptr)
        {
            const std::wstring quantityText = std::to_wstring(quantity);
            SetWindowTextW(g_optionQuantityInputs[index], quantityText.c_str());
        }
    }

    UpdateTotalPriceLabel();

    if (!OpenEditProductWindow(hMainWindow, g_selectedProductPath))
    {
        MessageBoxW(hDialogOwner, L"Projekt wurde geladen, aber das Bearbeitungsfenster konnte nicht geöffnet werden.", L"Projekt laden", MB_OK | MB_ICONWARNING);
    }

    return true;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PRODUCTCALCULATOR, szWindowClass, MAX_LOADSTRING);
    TryLoadAppSettings(g_appSettings);
    MyRegisterClass(hInstance);

    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PRODUCTCALCULATOR));

    MSG msg;

    while (GetMessage(&msg, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int) msg.wParam;
}

ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = hInstance;
    wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PRODUCTCALCULATOR));
    wcex.hCursor        = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_PRODUCTCALCULATOR);
    wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance;

   HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, 1000, 500, nullptr, nullptr, hInstance, nullptr);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   return TRUE;
}

// Zentraler Message-Dispatcher für alle Fenster-Ereignisse.
// Verarbeitet CREATE, SIZE, DESTROY, COMMAND und PAINT Nachrichten.
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        g_hProductDropdown = CreateWindowExW(
            0,
            L"COMBOBOX",
            nullptr,
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
            10,
            35,
            420,
            200,
            hWnd,
            reinterpret_cast<HMENU>(IDC_PRODUCT_DROPDOWN),
            hInst,
            nullptr);

        g_hEditProductButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Produkt bearbeiten",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            438,
            35,
            170,
            28,
            hWnd,
            reinterpret_cast<HMENU>(IDC_EDIT_PRODUCT_BUTTON),
            hInst,
            nullptr);

        g_hCreateProductButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Neues Produkt",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            616,
            35,
            170,
            28,
            hWnd,
            reinterpret_cast<HMENU>(IDC_CREATE_PRODUCT_BUTTON),
            hInst,
            nullptr);

        g_hDeleteProductButton = CreateWindowExW(
            0,
            L"BUTTON",
            L"Produkt löschen",
            WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
            794,
            35,
            170,
            28,
            hWnd,
            reinterpret_cast<HMENU>(IDC_DELETE_PRODUCT_BUTTON),
            hInst,
            nullptr);

        g_hCalcGroupBox = CreateWindowExW(
            0,
            L"BUTTON",
            L"Kalkulation",
            WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
            10,
            75,
            420,
            240,
            hWnd,
            reinterpret_cast<HMENU>(IDC_CALC_GROUPBOX),
            hInst,
            nullptr);

        const std::wstring initialBasePriceText = L"Basispreis: " + FormatPrice(0.0);
        const std::wstring initialTotalPriceText = L"Endpreis: " + FormatPrice(0.0);

        g_hBasePriceLabel = CreateWindowExW(
            0,
            L"STATIC",
            initialBasePriceText.c_str(),
            WS_CHILD | WS_VISIBLE,
            20,
            95,
            400,
            20,
            hWnd,
            reinterpret_cast<HMENU>(IDC_BASE_PRICE_LABEL),
            hInst,
            nullptr);

        g_hQuantityHeaderLabel = CreateWindowExW(
            0,
            L"STATIC",
            L"Anzahl",
            WS_CHILD | WS_VISIBLE,
            20,
            115,
            80,
            18,
            hWnd,
            reinterpret_cast<HMENU>(IDC_STATIC),
            hInst,
            nullptr);

        g_hTotalPriceLabel = CreateWindowExW(
            0,
            L"STATIC",
            initialTotalPriceText.c_str(),
            WS_CHILD | WS_VISIBLE,
            20,
            290,
            400,
            20,
            hWnd,
            reinterpret_cast<HMENU>(IDC_TOTAL_PRICE_LABEL),
            hInst,
            nullptr);

        g_hShowSummaryButton = CreateWindowExW(
            0, L"BUTTON", L"Ergebnis anzeigen",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            0, 0, 160, 28,
            hWnd, reinterpret_cast<HMENU>(IDC_BTN_SHOW_SUMMARY), hInst, nullptr);

        g_hLoadProjectButton = CreateWindowExW(
            0, L"BUTTON", L"Projekt laden",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
            0, 0, 160, 28,
            hWnd, reinterpret_cast<HMENU>(IDC_SUMMARY_LOAD_PROJECT), hInst, nullptr);

        if (g_hProductDropdown != nullptr)
        {
            PopulateProductDropdown(g_hProductDropdown);
            if (!g_selectedProductPath.empty())
            {
                RenderProductConfiguration(hWnd);
            }
        }

        CloseAppButton(hWnd);

        break;
    }
    case WM_SIZE:
    {
        int clientWidth = LOWORD(lParam);
        int clientHeight = HIWORD(lParam);

        UpdateLayout(hWnd, clientWidth, clientHeight);

        break;
    }
    case WM_PRODUCT_CONFIGURATION_UPDATED:
    {
        const std::wstring selectedPath = g_selectedProductPath;
        if (g_hProductDropdown != nullptr)
        {
            PopulateProductDropdown(g_hProductDropdown);
            if (!selectedPath.empty())
            {
                SelectProductByPath(g_hProductDropdown, selectedPath);
            }
        }

        if (!g_selectedProductPath.empty())
        {
            RenderProductConfiguration(hWnd);
        }

        return 0;
    }
    case WM_COMMAND:
    {
        const int wmId = LOWORD(wParam);
        const int wmEvent = HIWORD(wParam);

        if (wmId == IDC_PRODUCT_DROPDOWN && wmEvent == CBN_SELCHANGE)
        {
            const int selectedIndex = static_cast<int>(SendMessageW(g_hProductDropdown, CB_GETCURSEL, 0, 0));
            if (selectedIndex != CB_ERR)
            {
                const auto itemData = static_cast<size_t>(SendMessageW(g_hProductDropdown, CB_GETITEMDATA, selectedIndex, 0));
                if (itemData < g_productFolderPaths.size())
                {
                    g_selectedProductPath = g_productFolderPaths[itemData];
                    RenderProductConfiguration(hWnd);
                }
            }
            return 0;
        }

        if (wmId >= IDC_OPTION_QUANTITY_BASE && wmId < IDC_OPTION_QUANTITY_BASE + static_cast<int>(g_optionQuantityInputs.size())
            && (wmEvent == EN_CHANGE || wmEvent == EN_UPDATE || wmEvent == EN_KILLFOCUS))
        {
            UpdateTotalPriceLabel();
            return 0;
        }

        if ((wmId == IDM_EDIT_PRODUCT) || (wmId == IDC_EDIT_PRODUCT_BUTTON && wmEvent == BN_CLICKED))
        {
            if (g_selectedProductPath.empty())
            {
                MessageBoxW(hWnd, L"Bitte wählen Sie zuerst ein Produkt aus.", L"Produkt bearbeiten", MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            if (!OpenEditProductWindow(hWnd, g_selectedProductPath))
            {
                MessageBoxW(hWnd, L"Das Bearbeitungsfenster konnte nicht geöffnet werden.", L"Fehler", MB_OK | MB_ICONERROR);
            }
            return 0;
        }

        if (wmId == IDC_CREATE_PRODUCT_BUTTON && wmEvent == BN_CLICKED)
        {
            std::filesystem::path newProductPath;
            const NewProductCreationStatus creationStatus = CreateNewProductFolderWithDefaults(hWnd, newProductPath);
            if (creationStatus == NewProductCreationStatus::Cancelled)
            {
                return 0;
            }

            if (creationStatus != NewProductCreationStatus::Created || newProductPath.empty())
            {
                MessageBoxW(hWnd, L"Neues Produkt konnte nicht erstellt werden.", L"Fehler", MB_OK | MB_ICONERROR);
                return 0;
            }

            if (g_hProductDropdown != nullptr)
            {
                PopulateProductDropdown(g_hProductDropdown);
                SelectProductByPath(g_hProductDropdown, newProductPath.wstring());
            }

            if (!g_selectedProductPath.empty())
            {
                RenderProductConfiguration(hWnd);
            }

            if (!OpenEditProductWindow(hWnd, newProductPath.wstring()))
            {
                MessageBoxW(hWnd, L"Das Bearbeitungsfenster konnte nicht geöffnet werden.", L"Fehler", MB_OK | MB_ICONERROR);
            }
            return 0;
        }

        if (wmId == IDC_DELETE_PRODUCT_BUTTON && wmEvent == BN_CLICKED)
        {
            if (g_selectedProductPath.empty())
            {
                MessageBoxW(hWnd, L"Bitte wählen Sie zuerst ein Produkt aus.", L"Produkt löschen", MB_OK | MB_ICONINFORMATION);
                return 0;
            }

            const ProductDeletionStatus deletionStatus = ConfirmAndDeleteProduct(hWnd, g_selectedProductPath);
            if (deletionStatus != ProductDeletionStatus::Deleted)
            {
                return 0;
            }

            if (g_hProductDropdown != nullptr)
            {
                PopulateProductDropdown(g_hProductDropdown);
            }

            if (!g_selectedProductPath.empty())
            {
                RenderProductConfiguration(hWnd);
            }
            else
            {
                ClearDynamicOptionControls();
                g_currentConfiguration = ProductConfiguration{};
                SetWindowTextW(g_hBasePriceLabel, (L"Basispreis: " + FormatPrice(0.0)).c_str());
                SetWindowTextW(g_hTotalPriceLabel, (L"Endpreis: " + FormatPrice(0.0)).c_str());
            }

            return 0;
        }

        if (wmId == IDC_BTN_SHOW_SUMMARY)
        {
            ShowSummaryWindow(hWnd);
            return 0;
        }

        if (wmId == IDC_SUMMARY_LOAD_PROJECT)
        {
            LoadProjectIntoCalculator(hWnd, hWnd);
            return 0;
        }

        switch (wmId)
        {
        case IDM_ABOUT:
            DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
            break;
        case IDM_EXIT:
            DestroyWindow(hWnd);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
        }
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        SetBkMode(hdc, TRANSPARENT);

        RECT rc;
        rc.left = 10;
        rc.top = 10;
        rc.right = 700;
        rc.bottom = 30;

        const wchar_t* text = L"Bitte wählen Sie ein zu kalkulierendes Produkt aus.";
        DrawTextW(hdc, text, -1, &rc, DT_SINGLELINE | DT_LEFT | DT_VCENTER);

        EndPaint(hWnd, &ps);
    }
    break;
    case WM_DESTROY:
    {
        ClearDynamicOptionControls();
        g_productFolderPaths.clear();
        g_selectedProductPath.clear();
        g_currentConfiguration = ProductConfiguration{};
        g_hProductDropdown = nullptr;
        g_hEditProductButton = nullptr;
        g_hCreateProductButton = nullptr;
        g_hDeleteProductButton = nullptr;
        g_hCalcGroupBox = nullptr;
        g_hBasePriceLabel = nullptr;
        g_hQuantityHeaderLabel = nullptr;
        g_hTotalPriceLabel = nullptr;
        g_hShowSummaryButton = nullptr;
        g_hLoadProjectButton = nullptr;
        PostQuitMessage(0);
        break;
    }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

// Liest die Menge aus einem Quantity-Input-Feld.
// Gibt 0 zurück, wenn das Feld leer ist oder eine ungültige Zahl enthält.
// Negative Werte werden auf 0 begrenzt.
int GetOptionQuantity(size_t index)
{
    if (index >= g_optionQuantityInputs.size() || g_optionQuantityInputs[index] == nullptr)
    {
        return 0;
    }

    wchar_t buffer[64] = {};
    GetWindowTextW(g_optionQuantityInputs[index], buffer, static_cast<int>(std::size(buffer)));

    try
    {
        int value = std::stoi(buffer);
        return (value < 0) ? 0 : value;
    }
    catch (...)
    {
        return 0;
    }
}
