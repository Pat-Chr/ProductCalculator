// ProductCalculator.cpp : Definiert den Einstiegspunkt für die Anwendung.
//

#include "framework.h"
#include "ProductCalculator.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

#define MAX_LOADSTRING 100

// Globale Variablen:
HINSTANCE hInst;                                // Aktuelle Instanz
WCHAR szTitle[MAX_LOADSTRING];                  // Titelleistentext
WCHAR szWindowClass[MAX_LOADSTRING];            // Der Klassenname des Hauptfensters.
HWND g_hProductDropdown = nullptr;
std::vector<std::wstring> g_productFolderPaths;
std::wstring g_selectedProductPath;

// Vorwärtsdeklarationen der in diesem Codemodul enthaltenen Funktionen:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);

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

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    // Globale Zeichenfolgen initialisieren
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_PRODUCTCALCULATOR, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // Anwendungsinitialisierung ausführen:
    if (!InitInstance (hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_PRODUCTCALCULATOR));

    MSG msg;

    // Hauptnachrichtenschleife:
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



//
//  FUNKTION: MyRegisterClass()
//
//  ZWECK: Registriert die Fensterklasse.
//
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

//
//   FUNKTION: InitInstance(HINSTANCE, int)
//
//   ZWECK: Speichert das Instanzenhandle und erstellt das Hauptfenster.
//
//   KOMMENTARE:
//
//        In dieser Funktion wird das Instanzenhandle in einer globalen Variablen gespeichert, und das
//        Hauptprogrammfenster wird erstellt und angezeigt.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   hInst = hInstance; // Instanzenhandle in der globalen Variablen speichern

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

//
//  FUNKTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  ZWECK: Verarbeitet Meldungen für das Hauptfenster.
//
//  WM_COMMAND  - Verarbeiten des Anwendungsmenüs
//  WM_PAINT    - Darstellen des Hauptfensters
//  WM_DESTROY  - Ausgeben einer Beendenmeldung und zurückkehren
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
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

        if (g_hProductDropdown != nullptr)
        {
            PopulateProductDropdown(g_hProductDropdown);
        }
        break;
    case WM_SIZE:
        if (g_hProductDropdown != nullptr)
        {
            MoveWindow(g_hProductDropdown, 10, 35, max(180, LOWORD(lParam) - 20), 200, TRUE);
        }
        break;
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
                    }
                }
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
        g_productFolderPaths.clear();
        g_selectedProductPath.clear();
        g_hProductDropdown = nullptr;
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

// Meldungshandler für Infofeld.
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
