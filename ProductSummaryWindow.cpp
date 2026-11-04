#include "framework.h"
#include "ProductCalculator.h"
#include "AppLocalization.h"

#include <filesystem>
#include <sstream>
#include <string>

namespace
{
	std::wstring g_summaryWindowText;

	std::wstring UiText(const std::wstring& key, const std::wstring& fallback)
	{
		AppLocalization::SetLanguageFromSettings(g_appSettings);
		return AppLocalization::Text(key, fallback);
	}

	std::wstring GetSelectedProductDisplayName()
	{
		if (!g_selectedProductPath.empty())
		{
			return std::filesystem::path(g_selectedProductPath).filename().wstring();
		}

		if (g_hProductDropdown != nullptr)
		{
			int sel = static_cast<int>(SendMessageW(g_hProductDropdown, CB_GETCURSEL, 0, 0));
			if (sel >= 0)
			{
				wchar_t text[256] = {};
				SendMessageW(g_hProductDropdown, CB_GETLBTEXT, static_cast<WPARAM>(sel), reinterpret_cast<LPARAM>(text));
				// Fallback to the visible dropdown text if the global path
				// has not been set yet (e.g. directly after initialization).
				return text;
			}
		}

		return UiText(L"summary.no_product", L"(kein Produkt)");
	}

	// Builds the exportable text in a stable, line-based format.
	// If additional price components are added later, they should be appended here.
	std::wstring BuildSummaryText()
	{
		std::wostringstream out;
		out << L"" << GetSelectedProductDisplayName() << L"\r\n";
		out << UiText(L"common.article_number", L"Artikelnummer") << L": " << g_currentConfiguration.articleNumber << L"\r\n";
		out << UiText(L"main.label.base_price", L"Basispreis") << L": " << FormatPrice(g_currentConfiguration.basePrice) << L"\r\n";
		out << L"\r\n" << UiText(L"summary.options", L"Optionen") << L":\r\n";

		double total = g_currentConfiguration.basePrice;

		for (size_t i = 0; i < g_currentConfiguration.options.size(); ++i)
		{
			const ProductOption& option = g_currentConfiguration.options[i];
			const int quantity = GetOptionQuantity(i);
			if (quantity <= 0)
			{
				continue;
			}

			const double lineTotal = static_cast<double>(quantity) * option.price;
			total += lineTotal;

			const std::wstring optionArticleNumber = option.articleNumber.empty() ? L"-" : option.articleNumber;
			out << L" - " << quantity
				<< L"x " << option.name
				<< L" (" << UiText(L"common.article_number_short", L"Art.-Nr.") << L": " << optionArticleNumber << L")"
				<< L": " << FormatPrice(lineTotal)
				<< L"\r\n";
		}

		out << L"\r\n" << UiText(L"main.label.total_price", L"Endpreis") << L": " << FormatPrice(total) << L"\r\n";
		return out.str();
	}

	LRESULT CALLBACK SummaryWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_CREATE:
		{
			CreateWindowExW(
				WS_EX_CLIENTEDGE, L"EDIT", g_summaryWindowText.c_str(),
				WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL | ES_READONLY,
				12, 12, 720, 420,
				hWnd, reinterpret_cast<HMENU>(IDC_SUMMARY_EDIT), hInst, nullptr);

			CreateWindowExW(
				0, L"BUTTON", UiText(L"project.button.save", L"Projekt speichern").c_str(),
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				12, 440, 190, 32,
				hWnd, reinterpret_cast<HMENU>(IDC_SUMMARY_SAVE_PROJECT), hInst, nullptr);

			CreateWindowExW(
				0, L"BUTTON", UiText(L"summary.button.copy", L"Kopieren").c_str(),
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				180, 440, 120, 32,
				hWnd, reinterpret_cast<HMENU>(IDC_SUMMARY_COPY), hInst, nullptr);

			CreateWindowExW(
				0, L"BUTTON", UiText(L"summary.button.close", L"Schließen").c_str(),
				WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
				612, 440, 120, 32,
				hWnd, reinterpret_cast<HMENU>(IDC_SUMMARY_CLOSE), hInst, nullptr);

			return 0;
		}
		case WM_SIZE:
		{
			const int width = LOWORD(lParam);
			const int height = HIWORD(lParam);

			HWND hEdit = GetDlgItem(hWnd, IDC_SUMMARY_EDIT);
			HWND hSave = GetDlgItem(hWnd, IDC_SUMMARY_SAVE_PROJECT);
			HWND hCopy = GetDlgItem(hWnd, IDC_SUMMARY_COPY);
			HWND hClose = GetDlgItem(hWnd, IDC_SUMMARY_CLOSE);

			const int margin = 12;
			const int buttonH = 32;
			const int smallButtonW = 120;
			const int largeButtonW = 190;
			const int buttonGap = 8;
			const int editH = height - (margin * 3) - buttonH;
			const int buttonTop = height - margin - buttonH;

			if (hEdit) MoveWindow(hEdit, margin, margin, width - (margin * 2), max(120, editH), TRUE);

			int nextLeft = margin;
			if (hSave)
			{
				MoveWindow(hSave, nextLeft, buttonTop, largeButtonW, buttonH, TRUE);
				nextLeft += largeButtonW + buttonGap;
			}
			if (hCopy)
			{
				MoveWindow(hCopy, nextLeft, buttonTop, smallButtonW, buttonH, TRUE);
			}
			if (hClose)
			{
				MoveWindow(hClose, width - margin - smallButtonW, buttonTop, smallButtonW, buttonH, TRUE);
			}
			return 0;
		}
		case WM_COMMAND:
		{
			const int id = LOWORD(wParam);
			if (id == IDC_SUMMARY_CLOSE)
			{
				DestroyWindow(hWnd);
				return 0;
			}

			if (id == IDC_SUMMARY_SAVE_PROJECT)
			{
				SaveCurrentProject(hWnd);
				return 0;
			}

			if (id == IDC_SUMMARY_COPY)
			{
				HWND hEdit = GetDlgItem(hWnd, IDC_SUMMARY_EDIT);
				const int len = GetWindowTextLengthW(hEdit);
				std::wstring text;
				text.resize(static_cast<size_t>(len));
				if (len > 0)
				{
					GetWindowTextW(hEdit, text.data(), len + 1);
				}

				if (OpenClipboard(hWnd))
				{
					EmptyClipboard();
					const size_t bytes = (text.size() + 1) * sizeof(wchar_t);
					HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, bytes);
					if (hMem)
					{
						void* ptr = GlobalLock(hMem);
						if (ptr)
						{
							memcpy(ptr, text.c_str(), bytes);
							GlobalUnlock(hMem);
							SetClipboardData(CF_UNICODETEXT, hMem);
						}
						else
						{
							GlobalFree(hMem);
						}
					}
					CloseClipboard();
				}
				return 0;
			}
			break;
		}
		case WM_DESTROY:
			return 0;
		}

		return DefWindowProcW(hWnd, message, wParam, lParam);
	}

	ATOM RegisterSummaryWindowClass(HINSTANCE hInstance)
	{
		static bool isRegistered = false;
		static ATOM atom = 0;

		if (isRegistered)
		{
			return atom;
		}

		WNDCLASSEXW wcex = {};
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = SummaryWndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_PRODUCTCALCULATOR));
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = L"ProductSummaryWindowClass";
		wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

		atom = RegisterClassExW(&wcex);
		isRegistered = true;
		return atom;
	}
}

void ShowSummaryWindow(HWND hParent)
{
	g_summaryWindowText = BuildSummaryText();

	RegisterSummaryWindowClass(hInst);

	HWND hSummary = CreateWindowExW(
		WS_EX_DLGMODALFRAME,
		L"ProductSummaryWindowClass",
		UiText(L"summary.title", L"Produktübersicht").c_str(),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		CW_USEDEFAULT, CW_USEDEFAULT, 760, 520,
		hParent,
		nullptr,
		hInst,
		nullptr);

	if (hSummary != nullptr)
	{
		ShowWindow(hSummary, SW_SHOW);
		UpdateWindow(hSummary);
		SetForegroundWindow(hSummary);
	}
}
