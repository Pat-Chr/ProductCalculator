#include "framework.h"
#include "NewProductCreator.h"
#include "Resource.h"

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace
{
	struct NewProductDialogState
	{
		std::wstring* productName = nullptr;
	};

	std::wstring Trim(const std::wstring& value)
	{
		const size_t first = value.find_first_not_of(L" \t\r\n");
		if (first == std::wstring::npos)
		{
			return L"";
		}

		const size_t last = value.find_last_not_of(L" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	std::wstring SanitizeFolderName(const std::wstring& rawName)
	{
		std::wstring result;
		result.reserve(rawName.size());

		for (wchar_t ch : rawName)
		{
			switch (ch)
			{
			case L'<':
			case L'>':
			case L':':
			case L'"':
			case L'/':
			case L'\\':
			case L'|':
			case L'?':
			case L'*':
				result += L'_';
				break;
			default:
				result += ch;
				break;
			}
		}

		while (!result.empty() && (result.back() == L'.' || result.back() == L' '))
		{
			result.pop_back();
		}

		return Trim(result);
	}

	std::wstring BuildProductId(const std::wstring& productName)
	{
		std::wstring id;
		id.reserve(productName.size());
		bool previousWasSeparator = false;

		for (wchar_t ch : productName)
		{
			if (iswalnum(ch) != 0)
			{
				id += static_cast<wchar_t>(towlower(ch));
				previousWasSeparator = false;
			}
			else if (ch == L' ' || ch == L'-' || ch == L'_')
			{
				if (!previousWasSeparator && !id.empty())
				{
					id += L'-';
					previousWasSeparator = true;
				}
			}
		}

		while (!id.empty() && id.back() == L'-')
		{
			id.pop_back();
		}

		if (id.empty())
		{
			id = L"produkt";
		}

		return id;
	}

	std::string ToUtf8String(const std::wstring& text)
	{
		if (text.empty())
		{
			return "";
		}

		const int requiredSize = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
		if (requiredSize <= 0)
		{
			return "";
		}

		std::string result(requiredSize, '\0');
		WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), requiredSize, nullptr, nullptr);
		return result;
	}

	std::string EscapeJsonString(const std::string& text)
	{
		std::string escaped;
		escaped.reserve(text.size());

		for (const char ch : text)
		{
			switch (ch)
			{
			case '\\':
				escaped += "\\\\";
				break;
			case '"':
				escaped += "\\\"";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\r':
				escaped += "\\r";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				escaped += ch;
				break;
			}
		}

		return escaped;
	}

	std::filesystem::path FindProductDataDirectoryForCreation()
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

	INT_PTR CALLBACK NewProductNameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_INITDIALOG:
		{
			SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
			SendDlgItemMessageW(hDlg, IDC_NEW_PRODUCT_NAME, EM_LIMITTEXT, 120, 0);
			SetFocus(GetDlgItem(hDlg, IDC_NEW_PRODUCT_NAME));
			return FALSE;
		}
		case WM_COMMAND:
		{
			const int controlId = LOWORD(wParam);
			if (controlId == IDOK)
			{
				wchar_t buffer[256] = {};
				GetDlgItemTextW(hDlg, IDC_NEW_PRODUCT_NAME, buffer, static_cast<int>(std::size(buffer)));

				const std::wstring enteredName = Trim(buffer);
				if (enteredName.empty())
				{
					MessageBoxW(hDlg, L"Bitte einen Produktnamen eingeben.", L"Neues Produkt", MB_OK | MB_ICONINFORMATION);
					return TRUE;
				}

				auto* state = reinterpret_cast<NewProductDialogState*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
				if (state == nullptr || state->productName == nullptr)
				{
					EndDialog(hDlg, IDCANCEL);
					return TRUE;
				}

				*state->productName = enteredName;
				EndDialog(hDlg, IDOK);
				return TRUE;
			}

			if (controlId == IDCANCEL)
			{
				EndDialog(hDlg, IDCANCEL);
				return TRUE;
			}
			break;
		}
		}

		return FALSE;
	}

	bool TryPromptForProductName(HWND ownerWindow, std::wstring& productName)
	{
		NewProductDialogState state;
		state.productName = &productName;

		const INT_PTR dialogResult = DialogBoxParamW(
			GetModuleHandleW(nullptr),
			MAKEINTRESOURCEW(IDD_NEW_PRODUCT_DIALOG),
			ownerWindow,
			NewProductNameDialogProc,
			reinterpret_cast<LPARAM>(&state));

		return dialogResult == IDOK;
	}
}

NewProductCreationStatus CreateNewProductFolderWithDefaults(HWND ownerWindow, std::filesystem::path& createdProductPath)
{
	createdProductPath.clear();

	std::wstring enteredProductName;
	if (!TryPromptForProductName(ownerWindow, enteredProductName))
	{
		return NewProductCreationStatus::Cancelled;
	}

	const std::wstring folderName = SanitizeFolderName(enteredProductName);
	if (folderName.empty())
	{
		MessageBoxW(ownerWindow, L"Der Produktname enthält keine gültigen Zeichen für einen Ordnernamen.", L"Neues Produkt", MB_OK | MB_ICONERROR);
		return NewProductCreationStatus::Failed;
	}

	const std::filesystem::path productDataPath = FindProductDataDirectoryForCreation();
	if (productDataPath.empty())
	{
		return NewProductCreationStatus::Failed;
	}

	const std::filesystem::path candidatePath = productDataPath / folderName;
	std::error_code existsError;
	if (std::filesystem::exists(candidatePath, existsError))
	{
		MessageBoxW(ownerWindow, L"Ein Produkt mit diesem Namen existiert bereits.", L"Neues Produkt", MB_OK | MB_ICONINFORMATION);
		return NewProductCreationStatus::Failed;
	}

	std::error_code createError;
	if (!std::filesystem::create_directory(candidatePath, createError) || createError)
	{
		return NewProductCreationStatus::Failed;
	}

	const std::wstring productId = BuildProductId(enteredProductName);
	const std::string utf8ProductId = EscapeJsonString(ToUtf8String(productId));
	const std::string utf8ProductName = EscapeJsonString(ToUtf8String(enteredProductName));

	const std::filesystem::path configPath = candidatePath / L"produkt.json";
	std::ofstream file(configPath, std::ios::binary | std::ios::trunc);
	if (!file)
	{
		std::error_code removeError;
		std::filesystem::remove_all(candidatePath, removeError);
		return NewProductCreationStatus::Failed;
	}

	file << "{\n"
		<< "  \"produktId\": \"" << utf8ProductId << "\",\n"
		<< "  \"produktname\": \"" << utf8ProductName << "\",\n"
		<< "  \"basispreis\": 0.0,\n"
		<< "  \"optionen\": []\n"
		<< "}\n";

	if (!file)
	{
		file.close();
		std::error_code removeError;
		std::filesystem::remove_all(candidatePath, removeError);
		return NewProductCreationStatus::Failed;
	}

	createdProductPath = candidatePath;
	return NewProductCreationStatus::Created;
}
