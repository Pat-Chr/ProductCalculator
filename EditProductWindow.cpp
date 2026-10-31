#include "framework.h"
#include "EditProductWindow.h"
#include "Resource.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace
{
	constexpr wchar_t kEditWindowClass[] = L"ProductCalculatorEditWindow";
	HWND g_hEditWindow = nullptr;

	struct EditWindowState
	{
		std::wstring productFolderPath;
		HWND ownerWindow = nullptr;
		HWND editorControl = nullptr;
		HWND saveButton = nullptr;
		HWND cancelButton = nullptr;
	};

	std::wstring ToWideStringLocal(const std::string& text)
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

	std::string ToUtf8StringLocal(const std::wstring& text)
	{
		if (text.empty())
		{
			return "";
		}

		int requiredSize = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (requiredSize <= 0)
		{
			return "";
		}

		std::string result(requiredSize, '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), requiredSize, nullptr, nullptr);
		return result;
	}

	bool TryLoadProductJson(const std::wstring& productFolderPath, std::wstring& jsonText)
	{
		const std::filesystem::path configPath = std::filesystem::path(productFolderPath) / L"produkt.json";
		std::ifstream file(configPath, std::ios::binary);
		if (!file)
		{
			return false;
		}

		const std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		jsonText = ToWideStringLocal(content);
		return true;
	}

	bool TrySaveProductJson(const std::wstring& productFolderPath, const std::wstring& jsonText)
	{
		const std::filesystem::path configPath = std::filesystem::path(productFolderPath) / L"produkt.json";
		std::ofstream file(configPath, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}

		const std::string utf8Content = ToUtf8StringLocal(jsonText);
		file.write(utf8Content.data(), static_cast<std::streamsize>(utf8Content.size()));
		return static_cast<bool>(file);
	}

	void LayoutEditWindowControls(HWND hWnd, EditWindowState* state, int clientWidth, int clientHeight)
	{
		if (state == nullptr)
		{
			return;
		}

		const int margin = 10;
		const int buttonWidth = 110;
		const int buttonHeight = 28;
		const int buttonTop = max(margin, clientHeight - buttonHeight - margin);
		const int editorHeight = max(80, buttonTop - (margin * 2));

		if (state->editorControl != nullptr)
		{
			MoveWindow(state->editorControl, margin, margin, max(100, clientWidth - (margin * 2)), editorHeight, TRUE);
		}

		if (state->saveButton != nullptr)
		{
			MoveWindow(state->saveButton, max(margin, clientWidth - (buttonWidth * 2) - (margin * 2)), buttonTop, buttonWidth, buttonHeight, TRUE);
		}

		if (state->cancelButton != nullptr)
		{
			MoveWindow(state->cancelButton, max(margin, clientWidth - buttonWidth - margin), buttonTop, buttonWidth, buttonHeight, TRUE);
		}
	}

	void SaveAndRefresh(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr || state->editorControl == nullptr)
		{
			return;
		}

		const int textLength = GetWindowTextLengthW(state->editorControl);
		std::wstring jsonText(static_cast<size_t>(textLength + 1), L'\0');
		if (textLength > 0)
		{
			GetWindowTextW(state->editorControl, jsonText.data(), textLength + 1);
			jsonText.resize(static_cast<size_t>(textLength));
		}
		else
		{
			jsonText.clear();
		}

		if (!TrySaveProductJson(state->productFolderPath, jsonText))
		{
			MessageBoxW(hWnd, L"Produktdaten konnten nicht gespeichert werden.", L"Fehler", MB_OK | MB_ICONERROR);
			return;
		}

		if (state->ownerWindow != nullptr)
		{
			SendMessageW(state->ownerWindow, WM_PRODUCT_CONFIGURATION_UPDATED, 0, 0);
		}

		MessageBoxW(hWnd, L"Produkt wurde gespeichert und aktualisiert.", L"Produkt bearbeiten", MB_OK | MB_ICONINFORMATION);
	}

	LRESULT CALLBACK EditWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		EditWindowState* state = reinterpret_cast<EditWindowState*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

		switch (message)
		{
		case WM_CREATE:
		{
			CREATESTRUCTW* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
			state = reinterpret_cast<EditWindowState*>(createStruct->lpCreateParams);
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
			const HINSTANCE appInstance = GetModuleHandleW(nullptr);

			state->editorControl = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | WS_VSCROLL | WS_HSCROLL,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PRODUCT_TEXT),
				appInstance,
				nullptr);

			state->saveButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Speichern",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PRODUCT_SAVE),
				appInstance,
				nullptr);

			state->cancelButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Schließen",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PRODUCT_CANCEL),
				appInstance,
				nullptr);

			std::wstring jsonText;
			if (!TryLoadProductJson(state->productFolderPath, jsonText))
			{
				MessageBoxW(hWnd, L"Die Produktkonfiguration konnte nicht geladen werden.", L"Fehler", MB_OK | MB_ICONERROR);
			}
			else
			{
				SetWindowTextW(state->editorControl, jsonText.c_str());
			}

			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			LayoutEditWindowControls(hWnd, state, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
			return 0;
		}
		case WM_SIZE:
			LayoutEditWindowControls(hWnd, state, LOWORD(lParam), HIWORD(lParam));
			return 0;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_EDIT_PRODUCT_SAVE)
			{
				SaveAndRefresh(hWnd, state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_PRODUCT_CANCEL)
			{
				DestroyWindow(hWnd);
				return 0;
			}
			break;
		case WM_CLOSE:
			DestroyWindow(hWnd);
			return 0;
		case WM_DESTROY:
			if (g_hEditWindow == hWnd)
			{
				g_hEditWindow = nullptr;
			}
			delete state;
			SetWindowLongPtrW(hWnd, GWLP_USERDATA, 0);
			return 0;
		default:
			break;
		}

		return DefWindowProcW(hWnd, message, wParam, lParam);
	}
}

bool OpenEditProductWindow(HWND ownerWindow, const std::wstring& productFolderPath)
{
	if (ownerWindow == nullptr || productFolderPath.empty())
	{
		return false;
	}

	if (g_hEditWindow != nullptr)
	{
		ShowWindow(g_hEditWindow, SW_RESTORE);
		SetForegroundWindow(g_hEditWindow);
		return true;
	}

	const HINSTANCE appInstance = GetModuleHandleW(nullptr);

	WNDCLASSEXW windowClass = {};
	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.lpfnWndProc = EditWindowProc;
	windowClass.hInstance = appInstance;
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
	windowClass.lpszClassName = kEditWindowClass;

	RegisterClassExW(&windowClass);

	EditWindowState* state = new EditWindowState();
	state->productFolderPath = productFolderPath;
	state->ownerWindow = ownerWindow;

	g_hEditWindow = CreateWindowExW(
		WS_EX_APPWINDOW,
		kEditWindowClass,
		L"Produkt bearbeiten",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		760,
		520,
		ownerWindow,
		nullptr,
		appInstance,
		state);

	if (g_hEditWindow == nullptr)
	{
		delete state;
		return false;
	}

	ShowWindow(g_hEditWindow, SW_SHOW);
	UpdateWindow(g_hEditWindow);
	return true;
}
