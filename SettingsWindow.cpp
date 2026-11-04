#include "framework.h"
#include "SettingsWindow.h"

#include "AppSettingsStore.h"
#include "LanguageStore.h"
#include "Resource.h"

#include <algorithm>
#include <array>
#include <cwctype>
#include <map>
#include <utility>

namespace
{
	struct LanguageOption
	{
		const wchar_t* code;
		const wchar_t* displayNameKey;
		const wchar_t* fallbackDisplayName;
	};

	constexpr std::array<LanguageOption, 2> kLanguageOptions =
	{{
		{L"de", L"language.de", L"Deutsch"},
		{L"en", L"language.en", L"English"}
	}};

	struct SettingsWindowState
	{
		AppSettings* settings = nullptr;
		std::map<std::wstring, std::wstring> languageResources;
	};

	std::wstring TrimWhitespace(const std::wstring& value)
	{
		std::wstring trimmed = value;
		trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](wchar_t ch) { return !iswspace(ch); }));
		trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](wchar_t ch) { return !iswspace(ch); }).base(), trimmed.end());
		return trimmed;
	}

	std::wstring NormalizeLanguageCode(const std::wstring& languageCode)
	{
		std::wstring normalized = TrimWhitespace(languageCode);
		std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](wchar_t ch) { return static_cast<wchar_t>(towlower(ch)); });

		for (const LanguageOption& option : kLanguageOptions)
		{
			if (normalized == option.code)
			{
				return normalized;
			}
		}

		return L"de";
	}

	void ApplyLocalizedSettingsTexts(HWND hDlg, const std::map<std::wstring, std::wstring>& resources)
	{
		SetWindowTextW(hDlg, GetLocalizedText(resources, L"settings.title", L"Einstellungen").c_str());
		SetWindowTextW(GetDlgItem(hDlg, IDC_SETTINGS_GROUP_GENERAL), GetLocalizedText(resources, L"settings.group.general", L"Allgemein").c_str());
		SetWindowTextW(GetDlgItem(hDlg, IDC_SETTINGS_LABEL_CURRENCY), GetLocalizedText(resources, L"settings.label.currency", L"Standardwährung:").c_str());
		SetWindowTextW(GetDlgItem(hDlg, IDC_SETTINGS_LABEL_LANGUAGE), GetLocalizedText(resources, L"settings.label.language", L"Sprache:").c_str());
		SetWindowTextW(GetDlgItem(hDlg, IDC_SETTINGS_HINT_FUTURE), GetLocalizedText(resources, L"settings.hint.more", L"Weitere Einstellungen können hier später ergänzt werden.").c_str());
		SetWindowTextW(GetDlgItem(hDlg, IDOK), GetLocalizedText(resources, L"settings.button.save", L"Speichern").c_str());
		SetWindowTextW(GetDlgItem(hDlg, IDCANCEL), GetLocalizedText(resources, L"settings.button.cancel", L"Abbrechen").c_str());
	}

	void PopulateLanguageCombo(HWND hDlg, const std::map<std::wstring, std::wstring>& resources, const std::wstring& selectedCode)
	{
		HWND hLanguageCombo = GetDlgItem(hDlg, IDC_SETTINGS_LANGUAGE);
		SendMessageW(hLanguageCombo, CB_RESETCONTENT, 0, 0);

		int selectedIndex = 0;
		for (size_t i = 0; i < kLanguageOptions.size(); ++i)
		{
			const LanguageOption& option = kLanguageOptions[i];
			const std::wstring displayText = GetLocalizedText(resources, option.displayNameKey, option.fallbackDisplayName);
			SendMessageW(hLanguageCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(displayText.c_str()));

			if (selectedCode == option.code)
			{
				selectedIndex = static_cast<int>(i);
			}
		}

		SendMessageW(hLanguageCombo, CB_SETCURSEL, static_cast<WPARAM>(selectedIndex), 0);
	}

	std::wstring GetSelectedLanguageCode(HWND hDlg)
	{
		const LRESULT selectedIndex = SendMessageW(GetDlgItem(hDlg, IDC_SETTINGS_LANGUAGE), CB_GETCURSEL, 0, 0);
		if (selectedIndex == CB_ERR || selectedIndex < 0 || selectedIndex >= static_cast<LRESULT>(kLanguageOptions.size()))
		{
			return L"de";
		}

		return kLanguageOptions[static_cast<size_t>(selectedIndex)].code;
	}

	void LoadResourcesForLanguage(const std::wstring& languageCode, std::map<std::wstring, std::wstring>& resources)
	{
		if (!LoadLanguageResources(languageCode, resources))
		{
			LoadLanguageResources(L"de", resources);
		}
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

				const std::wstring selectedLanguage = NormalizeLanguageCode(initState->settings->sprache);
				LoadResourcesForLanguage(selectedLanguage, initState->languageResources);
				ApplyLocalizedSettingsTexts(hDlg, initState->languageResources);
				PopulateLanguageCombo(hDlg, initState->languageResources, selectedLanguage);
			}

			return static_cast<INT_PTR>(TRUE);
		}
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDC_SETTINGS_LANGUAGE:
				if (HIWORD(wParam) == CBN_SELCHANGE && state != nullptr)
				{
					const std::wstring selectedLanguage = GetSelectedLanguageCode(hDlg);
					LoadResourcesForLanguage(selectedLanguage, state->languageResources);
					ApplyLocalizedSettingsTexts(hDlg, state->languageResources);
					PopulateLanguageCombo(hDlg, state->languageResources, selectedLanguage);
				}
				return static_cast<INT_PTR>(TRUE);
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
					const std::wstring title = GetLocalizedText(state->languageResources, L"settings.title", L"Einstellungen");
					const std::wstring message = GetLocalizedText(state->languageResources, L"settings.validation.currency_required", L"Bitte geben Sie eine Währung ein.");
					MessageBoxW(hDlg, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
					return static_cast<INT_PTR>(TRUE);
				}

				state->settings->standardwaehrung = currency;
				state->settings->sprache = GetSelectedLanguageCode(hDlg);
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
		std::map<std::wstring, std::wstring> resources;
		LoadResourcesForLanguage(NormalizeLanguageCode(editedSettings.sprache), resources);
		const std::wstring title = GetLocalizedText(resources, L"settings.title", L"Einstellungen");
		const std::wstring message = GetLocalizedText(resources, L"settings.error.save_failed", L"Die Einstellungen konnten nicht gespeichert werden.");
		MessageBoxW(hOwner, message.c_str(), title.c_str(), MB_OK | MB_ICONERROR);
		return false;
	}

	settings = std::move(editedSettings);
	return true;
}
