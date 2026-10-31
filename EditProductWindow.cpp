#include "framework.h"
#include "EditProductWindow.h"
#include "Resource.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

namespace
{
	constexpr wchar_t kEditWindowClass[] = L"ProductCalculatorEditWindow";
	HWND g_hEditWindow = nullptr;

	struct ProductOption
	{
		std::wstring name;
		double price = 0.0;
	};

	struct ProductEditorModel
	{
		std::wstring productId;
		std::wstring productName;
		double basePrice = 0.0;
		std::vector<ProductOption> options;
	};

	struct EditWindowState
	{
		std::wstring productFolderPath;
		HWND ownerWindow = nullptr;
		HWND optionListControl = nullptr;
		HWND optionNameInput = nullptr;
		HWND optionPriceInput = nullptr;
		HWND addOptionButton = nullptr;
		HWND removeOptionButton = nullptr;
		HWND saveButton = nullptr;
		HWND cancelButton = nullptr;
		ProductEditorModel model;
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

	std::wstring EscapeJsonString(const std::wstring& text)
	{
		std::wstring escaped;
		escaped.reserve(text.size());

		for (wchar_t ch : text)
		{
			switch (ch)
			{
			case L'\\':
				escaped += L"\\\\";
				break;
			case L'"':
				escaped += L"\\\"";
				break;
			case L'\n':
				escaped += L"\\n";
				break;
			case L'\r':
				escaped += L"\\r";
				break;
			case L'\t':
				escaped += L"\\t";
				break;
			default:
				escaped += ch;
				break;
			}
		}

		return escaped;
	}

	std::wstring UnescapeJsonString(const std::wstring& text)
	{
		std::wstring result;
		result.reserve(text.size());

		for (size_t i = 0; i < text.size(); ++i)
		{
			if (text[i] == L'\\' && i + 1 < text.size())
			{
				const wchar_t escaped = text[++i];
				switch (escaped)
				{
				case L'n':
					result += L'\n';
					break;
				case L'r':
					result += L'\r';
					break;
				case L't':
					result += L'\t';
					break;
				case L'\\':
					result += L'\\';
					break;
				case L'"':
					result += L'"';
					break;
				default:
					result += escaped;
					break;
				}
				continue;
			}

			result += text[i];
		}

		return result;
	}

	std::string FormatJsonNumberLocal(double value)
	{
		std::ostringstream stream;
		stream.setf(std::ios::fixed);
		stream << std::setprecision(10) << value;
		std::string text = stream.str();
		while (!text.empty() && text.back() == '0')
		{
			text.pop_back();
		}
		if (!text.empty() && text.back() == '.')
		{
			text.pop_back();
		}
		if (text.empty())
		{
			text = "0";
		}
		return text;
	}

	bool TryLoadProductJsonText(const std::wstring& productFolderPath, std::string& jsonText)
	{
		const std::filesystem::path configPath = std::filesystem::path(productFolderPath) / L"produkt.json";
		std::ifstream file(configPath, std::ios::binary);
		if (!file)
		{
			return false;
		}

		jsonText.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		return true;
	}

	bool TrySaveProductJsonText(const std::wstring& productFolderPath, const std::string& jsonText)
	{
		const std::filesystem::path configPath = std::filesystem::path(productFolderPath) / L"produkt.json";
		std::ofstream file(configPath, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			return false;
		}

		file.write(jsonText.data(), static_cast<std::streamsize>(jsonText.size()));
		return static_cast<bool>(file);
	}

	bool TryParseProductModel(const std::string& jsonText, ProductEditorModel& model)
	{
		ProductEditorModel parsedModel;
		parsedModel.productId = L"";
		parsedModel.productName = L"";
		parsedModel.basePrice = 0.0;

		try
		{
			std::smatch idMatch;
			if (std::regex_search(jsonText, idMatch, std::regex("\\\"produktId\\\"\\s*:\\s*\\\"([^\\\"\\\\]*(?:\\\\.[^\\\"\\\\]*)*)\\\"")))
			{
				parsedModel.productId = UnescapeJsonString(ToWideStringLocal(idMatch[1].str()));
			}

			std::smatch nameMatch;
			if (std::regex_search(jsonText, nameMatch, std::regex("\\\"produktname\\\"\\s*:\\s*\\\"([^\\\"\\\\]*(?:\\\\.[^\\\"\\\\]*)*)\\\"")))
			{
				parsedModel.productName = UnescapeJsonString(ToWideStringLocal(nameMatch[1].str()));
			}

			std::smatch basePriceMatch;
			if (std::regex_search(jsonText, basePriceMatch, std::regex("\\\"basispreis\\\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)")))
			{
				parsedModel.basePrice = std::stod(basePriceMatch[1].str());
			}

			const std::regex optionRegex(R"opt(\{\s*"name"\s*:\s*"([^"\\]*(?:\\.[^"\\]*)*)"\s*,\s*"preis"\s*:\s*(-?\d+(?:\.\d+)?)\s*\})opt");
			for (std::sregex_iterator it(jsonText.begin(), jsonText.end(), optionRegex); it != std::sregex_iterator(); ++it)
			{
				const std::smatch& match = *it;
				ProductOption option;
				option.name = UnescapeJsonString(ToWideStringLocal(match[1].str()));
				option.price = std::stod(match[2].str());
				parsedModel.options.push_back(option);
			}
		}
		catch (...)
		{
			return false;
		}

		model = std::move(parsedModel);
		return true;
	}

	std::string BuildProductJson(const ProductEditorModel& model)
	{
		std::wostringstream stream;
		stream << L"{\n";
		stream << L"  \"produktId\": \"" << EscapeJsonString(model.productId) << L"\",\n";
		stream << L"  \"produktname\": \"" << EscapeJsonString(model.productName) << L"\",\n";
		stream << L"  \"basispreis\": " << ToWideStringLocal(FormatJsonNumberLocal(model.basePrice)) << L",\n";
		stream << L"  \"optionen\": [\n";

		for (size_t i = 0; i < model.options.size(); ++i)
		{
			const ProductOption& option = model.options[i];
			stream << L"    {\n";
			stream << L"      \"name\": \"" << EscapeJsonString(option.name) << L"\",\n";
			stream << L"      \"preis\": " << ToWideStringLocal(FormatJsonNumberLocal(option.price)) << L"\n";
			stream << L"    }";
			if (i + 1 < model.options.size())
			{
				stream << L",";
			}
			stream << L"\n";
		}

		stream << L"  ]\n";
		stream << L"}\n";
		return ToUtf8StringLocal(stream.str());
	}

	std::wstring FormatOptionDisplayText(const ProductOption& option)
	{
		std::wostringstream stream;
		stream.setf(std::ios::fixed);
		stream << option.name << L" (+" << std::setprecision(2) << option.price << L")";
		return stream.str();
	}

	void RefreshOptionList(EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr)
		{
			return;
		}

		SendMessageW(state->optionListControl, LB_RESETCONTENT, 0, 0);
		for (const ProductOption& option : state->model.options)
		{
			const std::wstring itemText = FormatOptionDisplayText(option);
			SendMessageW(state->optionListControl, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));
		}
	}

	void LayoutEditWindowControls(EditWindowState* state, int clientWidth, int clientHeight)
	{
		if (state == nullptr)
		{
			return;
		}

		const int margin = 10;
		const int inputHeight = 24;
		const int smallButtonWidth = 110;
		const int actionButtonWidth = 120;
		const int buttonHeight = 28;

		const int actionTop = clientHeight - margin - buttonHeight;
		const int inputTop = actionTop - 40;
		const int listTop = 34;
		const int listBottom = inputTop - 10;
		const int listHeight = max(100, listBottom - listTop);
		const int availableWidth = max(260, clientWidth - (margin * 2));

		const int nameLabelWidth = 60;
		const int nameInputLeft = margin + nameLabelWidth + 2;
		const int priceGap = 8;
		const int priceLabelWidth = 45;
		const int priceInputGap = 10;
		const int priceInputWidth = 90;
		const int buttonGap = 8;
		const int totalRightControlsWidth =
			priceGap + priceLabelWidth + priceInputGap + priceInputWidth + buttonGap + smallButtonWidth + buttonGap + smallButtonWidth;
		const int nameWidth = max(170, availableWidth - (nameInputLeft - margin) - totalRightControlsWidth);

		HWND listLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_PRODUCT_TEXT);
		if (listLabel != nullptr)
		{
			MoveWindow(listLabel, margin, margin, availableWidth, 20, TRUE);
		}

		if (state->optionListControl != nullptr)
		{
			MoveWindow(state->optionListControl, margin, listTop, availableWidth, listHeight, TRUE);
		}

		HWND nameLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_OPTION_NAME + 100);
		if (nameLabel != nullptr)
		{
			MoveWindow(nameLabel, margin, inputTop, 60, inputHeight, TRUE);
		}

		if (state->optionNameInput != nullptr)
		{
			MoveWindow(state->optionNameInput, nameInputLeft, inputTop, nameWidth, inputHeight, TRUE);
		}

		const int priceLabelLeft = nameInputLeft + nameWidth + priceGap;
		HWND priceLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_OPTION_PRICE + 100);
		if (priceLabel != nullptr)
		{
			MoveWindow(priceLabel, priceLabelLeft, inputTop, priceLabelWidth, inputHeight, TRUE);
		}

		const int priceInputLeft = priceLabelLeft + priceLabelWidth + priceInputGap;
		if (state->optionPriceInput != nullptr)
		{
			MoveWindow(state->optionPriceInput, priceInputLeft, inputTop, priceInputWidth, inputHeight, TRUE);
		}

		const int addButtonLeft = priceInputLeft + priceInputWidth + buttonGap;
		if (state->addOptionButton != nullptr)
		{
			MoveWindow(state->addOptionButton, addButtonLeft, inputTop, smallButtonWidth, buttonHeight, TRUE);
		}

		if (state->removeOptionButton != nullptr)
		{
			MoveWindow(state->removeOptionButton, addButtonLeft + smallButtonWidth + buttonGap, inputTop, smallButtonWidth, buttonHeight, TRUE);
		}

		if (state->saveButton != nullptr)
		{
			MoveWindow(state->saveButton, max(margin, clientWidth - (actionButtonWidth * 2) - (margin * 2)), actionTop, actionButtonWidth, buttonHeight, TRUE);
		}

		if (state->cancelButton != nullptr)
		{
			MoveWindow(state->cancelButton, max(margin, clientWidth - actionButtonWidth - margin), actionTop, actionButtonWidth, buttonHeight, TRUE);
		}
	}

	bool TryReadPrice(HWND priceInput, double& price)
	{
		if (priceInput == nullptr)
		{
			return false;
		}

		wchar_t buffer[64] = {};
		GetWindowTextW(priceInput, buffer, static_cast<int>(std::size(buffer)));
		std::wstring text = buffer;
		if (text.empty())
		{
			return false;
		}

		for (wchar_t& ch : text)
		{
			if (ch == L',')
			{
				ch = L'.';
			}
		}

		try
		{
			price = std::stod(text);
		}
		catch (...)
		{
			return false;
		}

		return true;
	}

	void AddOptionFromInput(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr || state->optionNameInput == nullptr || state->optionPriceInput == nullptr)
		{
			return;
		}

		wchar_t nameBuffer[256] = {};
		GetWindowTextW(state->optionNameInput, nameBuffer, static_cast<int>(std::size(nameBuffer)));
		std::wstring name = nameBuffer;
		if (name.empty())
		{
			MessageBoxW(hWnd, L"Bitte einen Optionsnamen eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		double price = 0.0;
		if (!TryReadPrice(state->optionPriceInput, price))
		{
			MessageBoxW(hWnd, L"Bitte einen gültigen Preis eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		state->model.options.push_back(ProductOption{ name, price });
		RefreshOptionList(state);
		SetWindowTextW(state->optionNameInput, L"");
		SetWindowTextW(state->optionPriceInput, L"0");
		SetFocus(state->optionNameInput);
	}

	void RemoveSelectedOption(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr)
		{
			return;
		}

		const LRESULT selected = SendMessageW(state->optionListControl, LB_GETCURSEL, 0, 0);
		if (selected == LB_ERR)
		{
			MessageBoxW(hWnd, L"Bitte zuerst eine Option auswählen.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		const size_t selectedIndex = static_cast<size_t>(selected);
		if (selectedIndex >= state->model.options.size())
		{
			return;
		}

		state->model.options.erase(state->model.options.begin() + selectedIndex);
		RefreshOptionList(state);
	}

	void SaveAndRefresh(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr)
		{
			return;
		}

		const std::string jsonText = BuildProductJson(state->model);
		if (!TrySaveProductJsonText(state->productFolderPath, jsonText))
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

			CreateWindowExW(
				0,
				L"STATIC",
				L"Optionen",
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PRODUCT_TEXT),
				appInstance,
				nullptr);

			state->optionListControl = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"LISTBOX",
				L"",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_LIST),
				appInstance,
				nullptr);

			CreateWindowExW(
				0,
				L"STATIC",
				L"Name:",
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_NAME + 100),
				appInstance,
				nullptr);

			state->optionNameInput = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_NAME),
				appInstance,
				nullptr);

			CreateWindowExW(
				0,
				L"STATIC",
				L"Preis:",
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_PRICE + 100),
				appInstance,
				nullptr);

			state->optionPriceInput = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"0",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_PRICE),
				appInstance,
				nullptr);

			state->addOptionButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Option hinzufügen",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_ADD),
				appInstance,
				nullptr);

			state->removeOptionButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Zeile löschen",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_REMOVE),
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

			std::string jsonText;
			if (!TryLoadProductJsonText(state->productFolderPath, jsonText) || !TryParseProductModel(jsonText, state->model))
			{
				MessageBoxW(hWnd, L"Die Produktkonfiguration konnte nicht geladen werden.", L"Fehler", MB_OK | MB_ICONERROR);
			}
			else
			{
				RefreshOptionList(state);
			}

			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			LayoutEditWindowControls(state, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
			return 0;
		}
		case WM_SIZE:
			LayoutEditWindowControls(state, LOWORD(lParam), HIWORD(lParam));
			return 0;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_EDIT_OPTION_ADD)
			{
				AddOptionFromInput(hWnd, state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_REMOVE)
			{
				RemoveSelectedOption(hWnd, state);
				return 0;
			}

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
