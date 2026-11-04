#include "framework.h"
#include "SettingsWindow.h"

#include "AppSettingsStore.h"
#include "Resource.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace
{
	struct SettingsWindowState
	{
		AppSettings* settings = nullptr;
	};

	std::wstring TrimWhitespace(const std::wstring& value)
	{
		std::wstring trimmed = value;
		trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](wchar_t ch) { return !iswspace(ch); }));
		trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](wchar_t ch) { return !iswspace(ch); }).base(), trimmed.end());
		return trimmed;
	}

	INT_PTR CALLBACK SettingsDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		SettingsWindowState* state = reinterpret_cast<SettingsWindowState*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));

		switch (message)
		{
		case WM_INITDIALOG:
		{
			auto* initState = reinterpret_cast<SettingsWindowState*>(lParam);
			SetWindowLongPtrW(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(initState));

			if (initState != nullptr && initState->settings != nullptr)
			{
				SetWindowTextW(GetDlgItem(hDlg, IDC_SETTINGS_CURRENCY), initState->settings->standardwaehrung.c_str());
			}

			return static_cast<INT_PTR>(TRUE);
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDOK:
			{
				if (state == nullptr || state->settings == nullptr)
				{
					EndDialog(hDlg, IDCANCEL);
					return static_cast<INT_PTR>(TRUE);
				}

				wchar_t currencyBuffer[64] = {};
				GetWindowTextW(GetDlgItem(hDlg, IDC_SETTINGS_CURRENCY), currencyBuffer, static_cast<int>(_countof(currencyBuffer)));

				const std::wstring currency = TrimWhitespace(currencyBuffer);
				if (currency.empty())
				{
					MessageBoxW(hDlg, L"Bitte geben Sie eine Währung ein.", L"Einstellungen", MB_OK | MB_ICONINFORMATION);
					return static_cast<INT_PTR>(TRUE);
				}

				state->settings->standardwaehrung = currency;
				EndDialog(hDlg, IDOK);
				return static_cast<INT_PTR>(TRUE);
			}
			case IDCANCEL:
				EndDialog(hDlg, IDCANCEL);
				return static_cast<INT_PTR>(TRUE);
			default:
				break;
			}
			break;
		default:
			break;
		}

		return static_cast<INT_PTR>(FALSE);
	}
}

bool PromptAndSaveAppSettings(HWND hOwner, HINSTANCE hInstance, AppSettings& settings)
{
	AppSettings editedSettings = settings;
	SettingsWindowState dialogState{};
	dialogState.settings = &editedSettings;

	const INT_PTR result = DialogBoxParamW(
		hInstance,
		MAKEINTRESOURCEW(IDD_SETTINGS_DIALOG),
		hOwner,
		SettingsDialogProc,
		reinterpret_cast<LPARAM>(&dialogState));

	if (result != IDOK)
	{
		return false;
	}

	if (!SaveAppSettings(editedSettings))
	{
		MessageBoxW(hOwner, L"Die Einstellungen konnten nicht gespeichert werden.", L"Einstellungen", MB_OK | MB_ICONERROR);
		return false;
	}

	settings = std::move(editedSettings);
	return true;
}
