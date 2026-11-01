#include "framework.h"
#include "ProductCopyCreator.h"
#include "Resource.h"

#include <cwctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>

namespace
{
	struct ProductCopyDialogState
	{
		std::wstring* productName = nullptr;
		std::wstring dialogTitle;
		std::wstring initialName;
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

	std::filesystem::path FindProductDataDirectoryForCopy()
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

	bool TryUpdateCopiedProductJson(const std::filesystem::path& copiedProductPath, const std::wstring& newProductName)
	{
		const std::filesystem::path jsonPath = copiedProductPath / L"produkt.json";
		std::ifstream input(jsonPath, std::ios::binary);
		if (!input)
		{
			return false;
		}

		std::string jsonContent((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
		input.close();

		const std::string escapedName = EscapeJsonString(ToUtf8String(newProductName));
		const std::string escapedId = EscapeJsonString(ToUtf8String(BuildProductId(newProductName)));

		const std::regex namePattern(R"("produktname"\s*:\s*"(?:\\.|[^"\\])*")");
		const std::regex idPattern(R"("produktId"\s*:\s*"(?:\\.|[^"\\])*")");

		if (!std::regex_search(jsonContent, namePattern) || !std::regex_search(jsonContent, idPattern))
		{
			return false;
		}

		jsonContent = std::regex_replace(jsonContent, namePattern, "\"produktname\": \"" + escapedName + "\"");
		jsonContent = std::regex_replace(jsonContent, idPattern, "\"produktId\": \"" + escapedId + "\"");

		std::ofstream output(jsonPath, std::ios::binary | std::ios::trunc);
		if (!output)
		{
			return false;
		}

		output << jsonContent;
		return static_cast<bool>(output);
	}

	INT_PTR CALLBACK ProductCopyNameDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_INITDIALOG:
		{
			SetWindowLongPtrW(hDlg, GWLP_USERDATA, lParam);
			auto* state = reinterpret_cast<ProductCopyDialogState*>(lParam);
			if (state != nullptr)
			{
				if (!state->dialogTitle.empty())
				{
					SetWindowTextW(hDlg, state->dialogTitle.c_str());
				}

				if (!state->initialName.empty())
				{
					SetDlgItemTextW(hDlg, IDC_NEW_PRODUCT_NAME, state->initialName.c_str());
				}
			}

			SendDlgItemMessageW(hDlg, IDC_NEW_PRODUCT_NAME, EM_LIMITTEXT, 120, 0);
			SetFocus(GetDlgItem(hDlg, IDC_NEW_PRODUCT_NAME));
			SendDlgItemMessageW(hDlg, IDC_NEW_PRODUCT_NAME, EM_SETSEL, 0, -1);
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
					MessageBoxW(hDlg, L"Bitte einen Produktnamen eingeben.", L"Produkt kopieren", MB_OK | MB_ICONINFORMATION);
					return TRUE;
				}

				auto* state = reinterpret_cast<ProductCopyDialogState*>(GetWindowLongPtrW(hDlg, GWLP_USERDATA));
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

	bool TryPromptForCopyName(HWND ownerWindow, const std::wstring& sourceProductName, std::wstring& copiedProductName)
	{
		ProductCopyDialogState state;
		state.productName = &copiedProductName;
		state.dialogTitle = L"Produkt kopieren";
		state.initialName = sourceProductName + L" Kopie";

		const INT_PTR dialogResult = DialogBoxParamW(
			GetModuleHandleW(nullptr),
			MAKEINTRESOURCEW(IDD_NEW_PRODUCT_DIALOG),
			ownerWindow,
			ProductCopyNameDialogProc,
			reinterpret_cast<LPARAM>(&state));

		return dialogResult == IDOK;
	}
}

ProductCopyStatus CopyExistingProductWithNewName(HWND ownerWindow, const std::filesystem::path& sourceProductPath, std::filesystem::path& copiedProductPath)
{
	copiedProductPath.clear();

	std::error_code sourceCheckError;
	if (sourceProductPath.empty() || !std::filesystem::exists(sourceProductPath, sourceCheckError) || !std::filesystem::is_directory(sourceProductPath, sourceCheckError))
	{
		return ProductCopyStatus::Failed;
	}

	const std::wstring sourceProductName = sourceProductPath.filename().wstring();
	std::wstring enteredProductName;
	if (!TryPromptForCopyName(ownerWindow, sourceProductName, enteredProductName))
	{
		return ProductCopyStatus::Cancelled;
	}

	const std::wstring folderName = SanitizeFolderName(enteredProductName);
	if (folderName.empty())
	{
		MessageBoxW(ownerWindow, L"Der Produktname enthält keine gültigen Zeichen für einen Ordnernamen.", L"Produkt kopieren", MB_OK | MB_ICONERROR);
		return ProductCopyStatus::Failed;
	}

	const std::filesystem::path productDataPath = FindProductDataDirectoryForCopy();
	if (productDataPath.empty())
	{
		return ProductCopyStatus::Failed;
	}

	const std::filesystem::path candidatePath = productDataPath / folderName;
	std::error_code existsError;
	if (std::filesystem::exists(candidatePath, existsError))
	{
		MessageBoxW(ownerWindow, L"Ein Produkt mit diesem Namen existiert bereits.", L"Produkt kopieren", MB_OK | MB_ICONINFORMATION);
		return ProductCopyStatus::Failed;
	}

	std::error_code copyError;
	std::filesystem::copy(sourceProductPath, candidatePath, std::filesystem::copy_options::recursive, copyError);
	if (copyError)
	{
		return ProductCopyStatus::Failed;
	}

	if (!TryUpdateCopiedProductJson(candidatePath, enteredProductName))
	{
		std::error_code removeError;
		std::filesystem::remove_all(candidatePath, removeError);
		return ProductCopyStatus::Failed;
	}

	copiedProductPath = candidatePath;
	return ProductCopyStatus::Copied;
}
