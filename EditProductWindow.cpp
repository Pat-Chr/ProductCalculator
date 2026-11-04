#include "framework.h"
#include "EditProductWindow.h"
#include "ProductCalculator.h"
#include "Resource.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
	// Fensterklasse für das Bearbeitungsfenster.
	// Wird verwendet, um ein separates Dialogfenster für Produktbearbeitung zu erstellen.
	constexpr wchar_t kEditWindowClass[] = L"ProductCalculatorEditWindow";
	HWND g_hEditWindow = nullptr;

	// Struktur zur Darstellung einer einzelnen Option (z.B. "Schrauber (+5 EUR)").
	// Enthält den Optionsnamen, die optionale Artikelnummer und den zugehörigen Aufschlagpreis.
	struct ProductOption
	{
		std::wstring name;			// Name der Option (z.B. "Schrauber")
		std::wstring articleNumber = L"-";	// Artikelnummer oder "-" falls nicht gesetzt
		double price = 0.0;			// Preis für diese Option in der Standardwährung
	};

	// Modellstruktur für die Produktbearbeitung.
	// Enthält alle Daten, die im JSON gespeichert werden:
	// - produktId: eindeutige Identifikation des Produkts
	// - produktname: menschlich lesbarer Name
	// - artikelnummer: optionale Artikelnummer ("-" wenn nicht vorhanden)
	// - basispreis: Grundpreis ohne Optionen
	// - optionen: Liste aller verfügbaren Optionen mit Namen, Artikelnummern und Preisen
	struct ProductEditorModel
	{
		std::wstring productId;		// Eindeutige ID des Produkts (z.B. "PROD-001")
		std::wstring productName;	// Menschlicher Produktname (z.B. "Basispaket")
		std::wstring articleNumber = L"-";	// Artikelnummer oder "-" falls nicht gesetzt
		double basePrice = 0.0;		// Basispreis ohne Optionen
		std::vector<ProductOption> options;	// Liste aller Optionen
	};

	// Hilfsstruktur zum Verwalten des Zustands des Bearbeitungsfensters.
	// Enthält alle HWNDs der Controls und die aktuellen Daten.
	struct EditWindowState
	{
		std::wstring productFolderPath;	// Pfad zum Produktdaten-Verzeichnis
		HWND ownerWindow = nullptr;			// Referenz zum Hauptfenster (für Nachrichten)
		HWND optionListControl = nullptr;	// ListBox zur Anzeige aller Optionen
		HWND basePriceInput = nullptr;		// Eingabefeld für den Basispreis
		HWND articleNumberInput = nullptr;	// Eingabefeld für die Produkt-Artikelnummer
		HWND optionNameInput = nullptr;		// Eingabefeld für den Optionsnamen
		HWND optionArticleNumberInput = nullptr;	// Eingabefeld für die Options-Artikelnummer
		HWND optionPriceInput = nullptr;		// Eingabefeld für den Optionspreis
		HWND optionQuantityInput = nullptr;	// Eingabefeld für die Menge der ausgewählten Option
		HWND applyOptionQuantityButton = nullptr;	// Button zum Übernehmen der Menge
		HWND addOptionButton = nullptr;		// Button zum Hinzufügen einer neuen Option
		HWND updateOptionButton = nullptr;	// Button zum Ändern der ausgewählten Option
		HWND removeOptionButton = nullptr;	// Button zum Löschen der ausgewählten Option
		HWND moveOptionUpButton = nullptr;	// Button zum Verschieben der ausgewählten Option nach oben
		HWND moveOptionDownButton = nullptr;	// Button zum Verschieben der ausgewählten Option nach unten
		HWND saveButton = nullptr;			// Speichern-Button
		HWND cancelButton = nullptr;			// Schließen/Abbrechen-Button
		HWND showSummaryButton = nullptr;	// Öffnet die Zusammenfassung des geladenen Projekts
		HWND saveProjectButton = nullptr;	// Speichert den aktuellen Projektstand
		bool showProjectSaveButton = true;	// Sichtbarkeit des Projekt-Speichern-Buttons
		bool showProductSaveButton = true;	// Sichtbarkeit des Produkt-Speichern-Buttons
		ProductEditorModel model;			// Aktuelle Produktmodell-Daten
	};

	// Konvertiert einen UTF-8 String in einen WideString.
	// Fallback auf den lokalen Codepage, wenn UTF-8 nicht funktioniert.
	// Wird benötigt, um JSON-Strings aus Textdateien zu lesen.
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

	// Konvertiert einen WideString in einen UTF-8 String.
	// Wird benötigt, um JSON-Daten für die Speicherung in Textdateien vorzubereiten.
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

	// Escapt JSON-spezifische Sonderzeichen in einem WideString.
	// Verhindert Syntaxfehler im JSON-Output (z.B. Anführungszeichen, Backslashes).
	// Beispiel: "Mein Preis" wird zu \"Mein Preis\"
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

	// Entescapt JSON-spezifische Sonderzeichen in einem WideString.
	// Wird verwendet, um JSON-Strings beim Laden wiederherzustellen.
	// Beispiel: \"Mein Preis\" wird zurück zu "Mein Preis"
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

	// Formatiert einen doubles als JSON-kompatible Zeichenkette.
	// Entfernt unnötige Nachkommastellen (z.B. 10.000000 -> 10)
	// Verwendet keine Exponentennotation für bessere Lesbarkeit.
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

	// Lädt den Inhalt der produkt.json Datei als Text.
	// Gibt false zurück, wenn die Datei nicht gefunden oder nicht lesbar ist.
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

	// Speichert den JSON-Text in die produkt.json Datei.
	// Überschreibt den bestehenden Inhalt (trunc flag).
	// Gibt false zurück, wenn die Datei nicht geschrieben werden kann.
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

	// Parst das Produktmodell aus dem JSON-Text.
	// Extrahiert produktId, produktname, artikelnummer, basispreis und optionen mittels Regex.
	// Gibt false zurück, wenn der Parsing-Fehler auftritt (z.B. ungültiges JSON).
	// Hinweis: Bei komplexeren JSON-Strukturen sollte ein vollwertiger JSON-Parser verwendet werden.
	bool TryParseProductModel(const std::string& jsonText, ProductEditorModel& model)
	{
		ProductEditorModel parsedModel;
		parsedModel.productId = L"";
		parsedModel.productName = L"";
		parsedModel.articleNumber = L"-";
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

			std::smatch articleNumberMatch;
			if (std::regex_search(jsonText, articleNumberMatch, std::regex("\\\"artikelnummer\\\"\\s*:\\s*\\\"([^\\\"\\\\]*(?:\\\\.[^\\\"\\\\]*)*)\\\"")))
			{
				const std::wstring parsedArticleNumber = UnescapeJsonString(ToWideStringLocal(articleNumberMatch[1].str()));
				if (!parsedArticleNumber.empty())
				{
					parsedModel.articleNumber = parsedArticleNumber;
				}
			}

			std::smatch basePriceMatch;
			if (std::regex_search(jsonText, basePriceMatch, std::regex("\\\"basispreis\\\"\\s*:\\s*(-?\\d+(?:\\.\\d+)?)")))
			{
				parsedModel.basePrice = std::stod(basePriceMatch[1].str());
			}

			const std::regex optionObjectRegex(R"opt(\{\s*"name"\s*:[^\}]*"preis"\s*:[^\}]*\})opt");
			const std::regex optionNameRegex("\\\"name\\\"\\s*:\\s*\\\"([^\\\"\\\\]*(?:\\\\.[^\\\"\\\\]*)*)\\\"");
			const std::regex optionArticleNumberRegex("\\\"artikelnummer\\\"\\s*:\\s*\\\"([^\\\"\\\\]*(?:\\\\.[^\\\"\\\\]*)*)\\\"");
			const std::regex optionPriceRegex(R"("preis"\s*:\s*(-?\d+(?:\.\d+)?))");

			for (std::sregex_iterator it(jsonText.begin(), jsonText.end(), optionObjectRegex); it != std::sregex_iterator(); ++it)
			{
				const std::string optionObjectText = it->str();
				std::smatch optionNameMatch;
				std::smatch optionPriceMatch;
				if (!std::regex_search(optionObjectText, optionNameMatch, optionNameRegex) ||
					!std::regex_search(optionObjectText, optionPriceMatch, optionPriceRegex))
				{
					continue;
				}

				ProductOption option;
				option.name = UnescapeJsonString(ToWideStringLocal(optionNameMatch[1].str()));
				std::smatch optionArticleNumberMatch;
				if (std::regex_search(optionObjectText, optionArticleNumberMatch, optionArticleNumberRegex))
				{
					const std::wstring parsedOptionArticleNumber = UnescapeJsonString(ToWideStringLocal(optionArticleNumberMatch[1].str()));
					if (!parsedOptionArticleNumber.empty())
					{
						option.articleNumber = parsedOptionArticleNumber;
					}
				}
				option.price = std::stod(optionPriceMatch[1].str());
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

	// Baut den JSON-Text aus dem Produktmodell.
	// Die Feldnamen und Reihenfolge sind absichtlich stabil gehalten,
	// damit bestehende Produktdateien und externe Werkzeuge ohne Anpassung weiter funktionieren.
	std::string BuildProductJson(const ProductEditorModel& model)
	{
		std::wostringstream stream;
		stream << L"{\n";
		stream << L"  \"produktId\": \"" << EscapeJsonString(model.productId) << L"\",\n";
		stream << L"  \"produktname\": \"" << EscapeJsonString(model.productName) << L"\",\n";
		stream << L"  \"artikelnummer\": \"" << EscapeJsonString(model.articleNumber) << L"\",\n";
		stream << L"  \"basispreis\": " << ToWideStringLocal(FormatJsonNumberLocal(model.basePrice)) << L",\n";
		stream << L"  \"optionen\": [\n";

		for (size_t i = 0; i < model.options.size(); ++i)
		{
			const ProductOption& option = model.options[i];
			stream << L"    {\n";
			stream << L"      \"name\": \"" << EscapeJsonString(option.name) << L"\",\n";
			stream << L"      \"artikelnummer\": \"" << EscapeJsonString(option.articleNumber) << L"\",\n";
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

	// Erzeugt den Anzeige-Text für eine Option in der ListBox.
	std::wstring FormatOptionDisplayText(const ProductOption& option, int quantity, bool showQuantity)
	{
		std::wostringstream stream;
		stream.setf(std::ios::fixed);
		const std::wstring currency = GetCurrentCurrency();
		stream << option.name << L" | Art.-Nr.: " << option.articleNumber << L" (+" << std::setprecision(2) << option.price << L" " << currency << L")";
		if (showQuantity)
		{
			stream << L" | Menge: " << max(0, quantity);
		}
		return stream.str();
	}

	bool ShouldShowQuantityEditor(const EditWindowState* state)
	{
		return state != nullptr && !state->showProductSaveButton;
	}

	bool ShouldShowSummaryButton(const EditWindowState* state)
	{
		return state != nullptr && !state->showProductSaveButton;
	}

	void SyncSelectedOptionQuantityInput(EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr || state->optionQuantityInput == nullptr || !ShouldShowQuantityEditor(state))
		{
			return;
		}

		int quantity = 0;
		const LRESULT selected = SendMessageW(state->optionListControl, LB_GETCURSEL, 0, 0);
		if (selected != LB_ERR)
		{
			const size_t selectedIndex = static_cast<size_t>(selected);
			if (selectedIndex < g_optionQuantityInputs.size())
			{
				quantity = GetOptionQuantity(selectedIndex);
			}
		}

		const std::wstring quantityText = std::to_wstring(max(0, quantity));
		SetWindowTextW(state->optionQuantityInput, quantityText.c_str());
	}

	void SyncSelectedOptionEditorInputs(EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr || state->optionNameInput == nullptr || state->optionArticleNumberInput == nullptr || state->optionPriceInput == nullptr)
		{
			return;
		}

		const LRESULT selected = SendMessageW(state->optionListControl, LB_GETCURSEL, 0, 0);
		if (selected == LB_ERR)
		{
			SetWindowTextW(state->optionNameInput, L"");
			SetWindowTextW(state->optionArticleNumberInput, L"-");
			SetWindowTextW(state->optionPriceInput, L"0");
			return;
		}

		const size_t selectedIndex = static_cast<size_t>(selected);
		if (selectedIndex >= state->model.options.size())
		{
			return;
		}

		const ProductOption& option = state->model.options[selectedIndex];
		SetWindowTextW(state->optionNameInput, option.name.c_str());
		SetWindowTextW(state->optionArticleNumberInput, option.articleNumber.c_str());
		const std::wstring priceText = ToWideStringLocal(FormatJsonNumberLocal(option.price));
		SetWindowTextW(state->optionPriceInput, priceText.c_str());
	}

	// Aktualisiert die ListBox mit allen aktuellen Optionen.
	// Wird aufgerufen, wenn sich das Modell ändert (z.B. nach Hinzufügen/Löschen).
	void RefreshOptionList(EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr)
		{
			return;
		}

		const bool showQuantityEditor = ShouldShowQuantityEditor(state);
		const LRESULT previousSelection = SendMessageW(state->optionListControl, LB_GETCURSEL, 0, 0);
		SendMessageW(state->optionListControl, LB_RESETCONTENT, 0, 0);
		for (size_t index = 0; index < state->model.options.size(); ++index)
		{
			const int quantity = (index < g_optionQuantityInputs.size()) ? GetOptionQuantity(index) : 0;
			const std::wstring itemText = FormatOptionDisplayText(state->model.options[index], quantity, showQuantityEditor);
			SendMessageW(state->optionListControl, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(itemText.c_str()));
		}

		if (!state->model.options.empty())
		{
			size_t selectionIndex = 0;
			if (previousSelection != LB_ERR)
			{
				selectionIndex = static_cast<size_t>(previousSelection);
			}
			if (selectionIndex >= state->model.options.size())
			{
				selectionIndex = state->model.options.size() - 1;
			}
			SendMessageW(state->optionListControl, LB_SETCURSEL, static_cast<WPARAM>(selectionIndex), 0);
		}
		else
		{
			SendMessageW(state->optionListControl, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
		}

		SyncSelectedOptionEditorInputs(state);
		SyncSelectedOptionQuantityInput(state);
	}

	// Passt die Position und Größe aller Fenster-Controls an.
	// Berechnet das Layout basierend auf dem verfügbaren Platz im Fenster.
	// Wird bei WM_SIZE-Ereignissen und beim ersten Erstellen aufgerufen.
	void LayoutEditWindowControls(EditWindowState* state, int clientWidth, int clientHeight)
	{
		if (state == nullptr)
		{
			return;
		}

		const int margin = 10;
		const int inputHeight = 24;
		const int smallButtonWidth = 130;
		const int actionButtonWidth = 150;
		const int buttonHeight = 28;

		const bool showQuantityEditor = ShouldShowQuantityEditor(state);
		const bool showSummaryButton = ShouldShowSummaryButton(state);
		const int actionTop = clientHeight - margin - buttonHeight;
		const int optionInputTop = actionTop - 40;
		const int quantityInputTop = optionInputTop - 34;
		const int baseInputTop = margin;
		const int articleInputTop = baseInputTop + inputHeight + 6;
		const int optionsLabelTop = articleInputTop + inputHeight + 8;
		const int listTop = optionsLabelTop + 22;
		const int listBottom = (showQuantityEditor ? quantityInputTop : optionInputTop) - 10;
		const int listHeight = max(100, listBottom - listTop);
		const int availableWidth = max(260, clientWidth - (margin * 2));

		const int baseLabelWidth = 145;
		const int baseInputWidth = 120;
		const int articleLabelWidth = 145;
		const int articleInputWidth = 180;
		HWND basePriceLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_BASE_PRICE + 100);
		if (basePriceLabel != nullptr)
		{
			MoveWindow(basePriceLabel, margin, baseInputTop, baseLabelWidth, inputHeight, TRUE);
		}

		if (state->basePriceInput != nullptr)
		{
			MoveWindow(state->basePriceInput, margin + baseLabelWidth + 6, baseInputTop, baseInputWidth, inputHeight, TRUE);
		}

		HWND articleNumberLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_ARTICLE_NUMBER + 100);
		if (articleNumberLabel != nullptr)
		{
			MoveWindow(articleNumberLabel, margin, articleInputTop, articleLabelWidth, inputHeight, TRUE);
		}

		if (state->articleNumberInput != nullptr)
		{
			MoveWindow(state->articleNumberInput, margin + articleLabelWidth + 6, articleInputTop, articleInputWidth, inputHeight, TRUE);
		}

		HWND listLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_PRODUCT_TEXT);
		if (listLabel != nullptr)
		{
			MoveWindow(listLabel, margin, optionsLabelTop, availableWidth, 20, TRUE);
		}

		const int listArrowButtonWidth = 34;
		const int listArrowButtonHeight = 28;
		const int listArrowGap = 8;
		const int listArrowColumnWidth = listArrowButtonWidth + listArrowGap;
		const int listWidth = max(180, availableWidth - listArrowColumnWidth);
		if (state->optionListControl != nullptr)
		{
			MoveWindow(state->optionListControl, margin, listTop, listWidth, listHeight, TRUE);
		}

		const int listArrowLeft = margin + listWidth + listArrowGap;
		const int listArrowsTotalHeight = (listArrowButtonHeight * 2) + listArrowGap;
		const int listArrowTop = listTop + max(0, (listHeight - listArrowsTotalHeight) / 2);
		if (state->moveOptionUpButton != nullptr)
		{
			MoveWindow(state->moveOptionUpButton, listArrowLeft, listArrowTop, listArrowButtonWidth, listArrowButtonHeight, TRUE);
		}

		if (state->moveOptionDownButton != nullptr)
		{
			MoveWindow(state->moveOptionDownButton, listArrowLeft, listArrowTop + listArrowButtonHeight + listArrowGap, listArrowButtonWidth, listArrowButtonHeight, TRUE);
		}

		HWND quantityLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_OPTION_QUANTITY + 100);
		if (quantityLabel != nullptr)
		{
			ShowWindow(quantityLabel, showQuantityEditor ? SW_SHOW : SW_HIDE);
			if (showQuantityEditor)
			{
				MoveWindow(quantityLabel, margin, quantityInputTop, 60, inputHeight, TRUE);
			}
		}

		if (state->optionQuantityInput != nullptr)
		{
			ShowWindow(state->optionQuantityInput, showQuantityEditor ? SW_SHOW : SW_HIDE);
			if (showQuantityEditor)
			{
				MoveWindow(state->optionQuantityInput, margin + 64, quantityInputTop, 90, inputHeight, TRUE);
			}
		}

		if (state->applyOptionQuantityButton != nullptr)
		{
			ShowWindow(state->applyOptionQuantityButton, showQuantityEditor ? SW_SHOW : SW_HIDE);
			if (showQuantityEditor)
			{
				MoveWindow(state->applyOptionQuantityButton, margin + 164, quantityInputTop, 170, buttonHeight, TRUE);
			}
		}

		const int nameLabelWidth = 60;
		const int nameInputLeft = margin + nameLabelWidth + 2;
		const int optionArticleGap = 8;
		const int optionArticleLabelWidth = 72;
		const int optionArticleInputGap = 8;
		const int optionArticleInputWidth = 90;
		const int priceGap = 8;
		const int priceLabelWidth = 110;
		const int priceInputGap = 10;
		const int priceInputWidth = 90;
		const int buttonGap = 8;
		const int totalRightControlsWidth =
			optionArticleGap + optionArticleLabelWidth + optionArticleInputGap + optionArticleInputWidth +
			priceGap + priceLabelWidth + priceInputGap + priceInputWidth +
			buttonGap + smallButtonWidth + buttonGap + smallButtonWidth + buttonGap + smallButtonWidth;
		const int nameWidth = max(100, min(190, availableWidth - (nameInputLeft - margin) - totalRightControlsWidth));

		HWND nameLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_OPTION_NAME + 100);
		if (nameLabel != nullptr)
		{
			MoveWindow(nameLabel, margin, optionInputTop, 60, inputHeight, TRUE);
		}

		if (state->optionNameInput != nullptr)
		{
			MoveWindow(state->optionNameInput, nameInputLeft, optionInputTop, nameWidth, inputHeight, TRUE);
		}

		const int optionArticleLabelLeft = nameInputLeft + nameWidth + optionArticleGap;
		HWND optionArticleLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_OPTION_ARTICLE_NUMBER + 100);
		if (optionArticleLabel != nullptr)
		{
			MoveWindow(optionArticleLabel, optionArticleLabelLeft, optionInputTop, optionArticleLabelWidth, inputHeight, TRUE);
		}

		const int optionArticleInputLeft = optionArticleLabelLeft + optionArticleLabelWidth + optionArticleInputGap;
		if (state->optionArticleNumberInput != nullptr)
		{
			MoveWindow(state->optionArticleNumberInput, optionArticleInputLeft, optionInputTop, optionArticleInputWidth, inputHeight, TRUE);
		}

		const int priceLabelLeft = optionArticleInputLeft + optionArticleInputWidth + priceGap;
		HWND priceLabel = GetDlgItem(g_hEditWindow, IDC_EDIT_OPTION_PRICE + 100);
		if (priceLabel != nullptr)
		{
			MoveWindow(priceLabel, priceLabelLeft, optionInputTop, priceLabelWidth, inputHeight, TRUE);
		}

		const int priceInputLeft = priceLabelLeft + priceLabelWidth + priceInputGap;
		if (state->optionPriceInput != nullptr)
		{
			MoveWindow(state->optionPriceInput, priceInputLeft, optionInputTop, priceInputWidth, inputHeight, TRUE);
		}

		const int addButtonLeft = priceInputLeft + priceInputWidth + buttonGap;
		if (state->addOptionButton != nullptr)
		{
			MoveWindow(state->addOptionButton, addButtonLeft, optionInputTop, smallButtonWidth, buttonHeight, TRUE);
		}

		const int updateButtonLeft = addButtonLeft + smallButtonWidth + buttonGap;
		if (state->updateOptionButton != nullptr)
		{
			MoveWindow(state->updateOptionButton, updateButtonLeft, optionInputTop, smallButtonWidth, buttonHeight, TRUE);
		}

		const int removeButtonLeft = updateButtonLeft + smallButtonWidth + buttonGap;
		if (state->removeOptionButton != nullptr)
		{
			MoveWindow(state->removeOptionButton, removeButtonLeft, optionInputTop, smallButtonWidth, buttonHeight, TRUE);
		}


		const int bottomButtonGap = 8;
		const int bottomButtonCount = 1 + (showSummaryButton ? 1 : 0) + (state->showProjectSaveButton ? 1 : 0) + (state->showProductSaveButton ? 1 : 0);
		const int bottomRowWidth = (actionButtonWidth * bottomButtonCount) + (bottomButtonGap * (bottomButtonCount - 1));
		const int bottomStartLeft = max(margin, clientWidth - margin - bottomRowWidth);

		int buttonIndex = 0;
		if (state->showSummaryButton != nullptr)
		{
			ShowWindow(state->showSummaryButton, showSummaryButton ? SW_SHOW : SW_HIDE);
			if (showSummaryButton)
			{
				MoveWindow(state->showSummaryButton, bottomStartLeft + (actionButtonWidth + bottomButtonGap) * buttonIndex, actionTop, actionButtonWidth, buttonHeight, TRUE);
				++buttonIndex;
			}
		}

		if (state->showProjectSaveButton && state->saveProjectButton != nullptr)
		{
			MoveWindow(state->saveProjectButton, bottomStartLeft + (actionButtonWidth + bottomButtonGap) * buttonIndex, actionTop, actionButtonWidth, buttonHeight, TRUE);
			++buttonIndex;
		}

		if (state->showProductSaveButton && state->saveButton != nullptr)
		{
			MoveWindow(state->saveButton, bottomStartLeft + (actionButtonWidth + bottomButtonGap) * buttonIndex, actionTop, actionButtonWidth, buttonHeight, TRUE);
			++buttonIndex;
		}

		if (state->cancelButton != nullptr)
		{
			MoveWindow(state->cancelButton, bottomStartLeft + (actionButtonWidth + bottomButtonGap) * buttonIndex, actionTop, actionButtonWidth, buttonHeight, TRUE);
		}
	}

	// Versucht den Preis aus einem Eingabefeld zu lesen.
	// Konvertiert Kommas in Punkte (für deutsche Zahlenformate).
	// Gibt true zurück, wenn ein gültiger Preis gelesen wurde.
	// Gibt false zurück, wenn das Feld leer ist oder ungültige Daten enthält.
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

	bool TryReadQuantity(HWND quantityInput, int& quantity)
	{
		quantity = 0;
		if (quantityInput == nullptr)
		{
			return false;
		}

		wchar_t buffer[32] = {};
		GetWindowTextW(quantityInput, buffer, static_cast<int>(std::size(buffer)));
		std::wstring text = buffer;
		if (text.empty())
		{
			return false;
		}

		try
		{
			const int value = std::stoi(text);
			if (value < 0)
			{
				return false;
			}
			quantity = value;
		}
		catch (...)
		{
			return false;
		}

		return true;
	}

	std::wstring NormalizeArticleNumber(const std::wstring& rawValue)
	{
		const size_t first = rawValue.find_first_not_of(L" \t\r\n");
		if (first == std::wstring::npos)
		{
			return L"-";
		}

		const size_t last = rawValue.find_last_not_of(L" \t\r\n");
		const std::wstring trimmed = rawValue.substr(first, last - first + 1);
		return trimmed.empty() ? L"-" : trimmed;
	}

	void ApplySelectedOptionQuantity(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr || state->optionQuantityInput == nullptr || !ShouldShowQuantityEditor(state))
		{
			return;
		}

		const LRESULT selected = SendMessageW(state->optionListControl, LB_GETCURSEL, 0, 0);
		if (selected == LB_ERR)
		{
			MessageBoxW(hWnd, L"Bitte zuerst eine Option auswählen.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		int quantity = 0;
		if (!TryReadQuantity(state->optionQuantityInput, quantity))
		{
			MessageBoxW(hWnd, L"Bitte eine gültige Menge (0 oder größer) eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		const size_t selectedIndex = static_cast<size_t>(selected);
		if (selectedIndex >= g_optionQuantityInputs.size() || g_optionQuantityInputs[selectedIndex] == nullptr)
		{
			MessageBoxW(hWnd, L"Die Menge kann für diese Option aktuell nicht gesetzt werden.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		const std::wstring quantityText = std::to_wstring(quantity);
		SetWindowTextW(g_optionQuantityInputs[selectedIndex], quantityText.c_str());
		RefreshOptionList(state);
		SendMessageW(state->optionListControl, LB_SETCURSEL, static_cast<WPARAM>(selectedIndex), 0);
		SyncSelectedOptionQuantityInput(state);
	}

	// Fügt eine neue Option aus den Eingabefeldern hinzu.
	// Validiert, dass ein nicht-leerer Name eingegeben wurde.
	// Der Preis wird auf 0 gesetzt (Standardwert).
	// Aktualisiert die ListBox und setzt den Fokus zurück auf den Namen-Eingabe.
	void AddOptionFromInput(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr || state->optionNameInput == nullptr || state->optionArticleNumberInput == nullptr || state->optionPriceInput == nullptr)
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

		wchar_t articleNumberBuffer[256] = {};
		GetWindowTextW(state->optionArticleNumberInput, articleNumberBuffer, static_cast<int>(std::size(articleNumberBuffer)));
		const std::wstring articleNumber = NormalizeArticleNumber(articleNumberBuffer);

		double price = 0.0;
		if (!TryReadPrice(state->optionPriceInput, price))
		{
			MessageBoxW(hWnd, L"Bitte einen gültigen Preis eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		state->model.options.push_back(ProductOption{ name, articleNumber, price });
		RefreshOptionList(state);
		SetWindowTextW(state->optionNameInput, L"");
		SetWindowTextW(state->optionArticleNumberInput, L"-");
		// Der Preis wird bewusst auf 0 zurückgesetzt, damit neue Optionen
		// nicht versehentlich mit einem vorherigen Preis angelegt werden.
		SetWindowTextW(state->optionPriceInput, L"0");
		SetFocus(state->optionNameInput);
	}

	void UpdateSelectedOptionFromInput(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr || state->optionListControl == nullptr || state->optionNameInput == nullptr || state->optionArticleNumberInput == nullptr || state->optionPriceInput == nullptr)
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

		wchar_t nameBuffer[256] = {};
		GetWindowTextW(state->optionNameInput, nameBuffer, static_cast<int>(std::size(nameBuffer)));
		std::wstring name = nameBuffer;
		if (name.empty())
		{
			MessageBoxW(hWnd, L"Bitte einen Optionsnamen eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		wchar_t articleNumberBuffer[256] = {};
		GetWindowTextW(state->optionArticleNumberInput, articleNumberBuffer, static_cast<int>(std::size(articleNumberBuffer)));
		const std::wstring articleNumber = NormalizeArticleNumber(articleNumberBuffer);

		double price = 0.0;
		if (!TryReadPrice(state->optionPriceInput, price))
		{
			MessageBoxW(hWnd, L"Bitte einen gültigen Preis eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		state->model.options[selectedIndex].name = name;
		state->model.options[selectedIndex].articleNumber = articleNumber;
		state->model.options[selectedIndex].price = price;
		RefreshOptionList(state);
		SendMessageW(state->optionListControl, LB_SETCURSEL, static_cast<WPARAM>(selectedIndex), 0);
		SyncSelectedOptionEditorInputs(state);
		SyncSelectedOptionQuantityInput(state);
	}

	// Entfernt die aktuell ausgewählte Option aus der Liste.
	// Zeigt eine Meldung an, wenn keine Option ausgewählt ist.
	// Aktualisiert danach die ListBox.
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

	// Verschiebt die aktuell ausgewählte Option innerhalb der Liste.
	// Richtung: -1 nach oben, +1 nach unten.
	void MoveSelectedOption(HWND hWnd, EditWindowState* state, int direction)
	{
		if (state == nullptr || state->optionListControl == nullptr || direction == 0)
		{
			return;
		}

		const LRESULT selected = SendMessageW(state->optionListControl, LB_GETCURSEL, 0, 0);
		if (selected == LB_ERR)
		{
			MessageBoxW(hWnd, L"Bitte zuerst eine Option auswählen.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		const int selectedIndex = static_cast<int>(selected);
		const int targetIndex = selectedIndex + direction;
		if (targetIndex < 0 || targetIndex >= static_cast<int>(state->model.options.size()))
		{
			return;
		}

		std::swap(state->model.options[selectedIndex], state->model.options[targetIndex]);
		RefreshOptionList(state);
		SendMessageW(state->optionListControl, LB_SETCURSEL, static_cast<WPARAM>(targetIndex), 0);
	}

	// Speichert das Produkt und aktualisiert das Hauptfenster.
	// Baut den JSON-Text aus dem aktuellen Modell.
	// Schreibt die Daten in produkt.json.
	// Sendet eine WM_PRODUCT_CONFIGURATION_UPDATED-Nachricht an das Hauptfenster.
	// Zeigt eine Erfolgsmeldung an.
	void SaveAndRefresh(HWND hWnd, EditWindowState* state)
	{
		if (state == nullptr)
		{
			return;
		}

		double basePrice = 0.0;
		if (!TryReadPrice(state->basePriceInput, basePrice))
		{
			MessageBoxW(hWnd, L"Bitte einen gültigen Basispreis eingeben.", L"Hinweis", MB_OK | MB_ICONINFORMATION);
			return;
		}

		state->model.basePrice = basePrice;
		if (state->articleNumberInput != nullptr)
		{
			wchar_t articleNumberBuffer[256] = {};
			GetWindowTextW(state->articleNumberInput, articleNumberBuffer, static_cast<int>(std::size(articleNumberBuffer)));
			state->model.articleNumber = NormalizeArticleNumber(articleNumberBuffer);
		}
		else
		{
			state->model.articleNumber = L"-";
		}

		const std::string jsonText = BuildProductJson(state->model);
		if (!TrySaveProductJsonText(state->productFolderPath, jsonText))
		{
			MessageBoxW(hWnd, L"Produktdaten konnten nicht gespeichert werden.", L"Fehler", MB_OK | MB_ICONERROR);
			return;
		}

		if (state->ownerWindow != nullptr)
		{
			// Das Hauptfenster wird nach jedem Speichern sofort synchronisiert.
			// Zusätzliche abgeleitete UI-Ansichten sollten ebenfalls auf diese Nachricht hören.
			SendMessageW(state->ownerWindow, WM_PRODUCT_CONFIGURATION_UPDATED, 0, 0);
		}

		MessageBoxW(hWnd, L"Produkt wurde gespeichert und aktualisiert.", L"Produkt bearbeiten", MB_OK | MB_ICONINFORMATION);
	}

	// Message-Dispatcher für das Bearbeitungsfenster.
	// Verarbeitet CREATE, SIZE, COMMAND, CLOSE und DESTROY Nachrichten.
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
			const std::wstring currency = GetCurrentCurrency();
			const std::wstring basePriceLabelText = L"Basispreis (" + currency + L"):";
			const std::wstring articleNumberLabelText = L"Artikelnummer:";
			const std::wstring optionsLabelText = L"Optionen (" + currency + L")";
			const std::wstring optionArticleNumberLabelText = L"Art.-Nr.:";
			const std::wstring optionPriceLabelText = L"Preis (" + currency + L"):";

			// Erstellt das Label und Eingabefeld für den Basispreis.
			CreateWindowExW(
				0,
				L"STATIC",
				basePriceLabelText.c_str(),
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_BASE_PRICE + 100),
				appInstance,
				nullptr);

			state->basePriceInput = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"0",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_BASE_PRICE),
				appInstance,
				nullptr);

			CreateWindowExW(
				0,
				L"STATIC",
				articleNumberLabelText.c_str(),
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_ARTICLE_NUMBER + 100),
				appInstance,
				nullptr);

			state->articleNumberInput = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"-",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_ARTICLE_NUMBER),
				appInstance,
				nullptr);

			// Erstellt die ListBox zur Anzeige aller Optionen.
			CreateWindowExW(
				0,
				L"STATIC",
				optionsLabelText.c_str(),
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
				L"Menge:",
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_QUANTITY + 100),
				appInstance,
				nullptr);

			state->optionQuantityInput = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"0",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT | ES_NUMBER,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_QUANTITY),
				appInstance,
				nullptr);

			state->applyOptionQuantityButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Menge übernehmen",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_SET_QUANTITY),
				appInstance,
				nullptr);

			// Erstellt das Label und Eingabefeld für den Optionsnamen.
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
				optionArticleNumberLabelText.c_str(),
				WS_CHILD | WS_VISIBLE,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_ARTICLE_NUMBER + 100),
				appInstance,
				nullptr);

			state->optionArticleNumberInput = CreateWindowExW(
				WS_EX_CLIENTEDGE,
				L"EDIT",
				L"-",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_LEFT,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_ARTICLE_NUMBER),
				appInstance,
				nullptr);

			// Erstellt das Label und Eingabefeld für den Optionspreis.
			CreateWindowExW(
				0,
				L"STATIC",
				optionPriceLabelText.c_str(),
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

			// Erstellt den Button zum Hinzufügen einer neuen Option.
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

			// Erstellt den Button zum Ändern der ausgewählten Option.
			state->updateOptionButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Option ändern",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_UPDATE),
				appInstance,
				nullptr);

			// Erstellt den Button zum Löschen der ausgewählten Option.
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

			// Erstellt den Button zum Verschieben der ausgewählten Option nach oben.
			state->moveOptionUpButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"↑",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_MOVE_UP),
				appInstance,
				nullptr);

			// Erstellt den Button zum Verschieben der ausgewählten Option nach unten.
			state->moveOptionDownButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"↓",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_OPTION_MOVE_DOWN),
				appInstance,
				nullptr);

			// Erstellt den Button zum Öffnen der Projektzusammenfassung.
			state->showSummaryButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Kalkulieren",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PROJECT_TO_SUMMARY),
				appInstance,
				nullptr);
			if (state->showSummaryButton != nullptr)
			{
				ShowWindow(state->showSummaryButton, ShouldShowSummaryButton(state) ? SW_SHOW : SW_HIDE);
			}

			// Erstellt den Button zum Speichern des aktuellen Projektstands.
			state->saveProjectButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Projekt speichern",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PROJECT_SAVE_CHANGES),
				appInstance,
				nullptr);
			if (state->saveProjectButton != nullptr)
			{
				ShowWindow(state->saveProjectButton, state->showProjectSaveButton ? SW_SHOW : SW_HIDE);
			}

			// Erstellt den Speichern-Button (Standardbutton).
			state->saveButton = CreateWindowExW(
				0,
				L"BUTTON",
				L"Produkt speichern",
				WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
				0,
				0,
				0,
				0,
				hWnd,
				reinterpret_cast<HMENU>(IDC_EDIT_PRODUCT_SAVE),
				appInstance,
				nullptr);
			if (state->saveButton != nullptr)
			{
				ShowWindow(state->saveButton, state->showProductSaveButton ? SW_SHOW : SW_HIDE);
			}

			// Erstellt den Schließen/Abbrechen-Button.
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
				if (state->basePriceInput != nullptr)
				{
					const std::wstring basePriceText = ToWideStringLocal(FormatJsonNumberLocal(state->model.basePrice));
					SetWindowTextW(state->basePriceInput, basePriceText.c_str());
				}
				if (state->articleNumberInput != nullptr)
				{
					SetWindowTextW(state->articleNumberInput, state->model.articleNumber.c_str());
				}
				RefreshOptionList(state);
			}

			RECT clientRect;
			GetClientRect(hWnd, &clientRect);
			LayoutEditWindowControls(state, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
			return 0;
		}
		case WM_SIZE:
			// Passt das Layout an, wenn sich die Fenstergröße ändert.
			LayoutEditWindowControls(state, LOWORD(lParam), HIWORD(lParam));
			return 0;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_EDIT_OPTION_LIST && HIWORD(wParam) == LBN_SELCHANGE)
			{
				SyncSelectedOptionEditorInputs(state);
				SyncSelectedOptionQuantityInput(state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_SET_QUANTITY)
			{
				ApplySelectedOptionQuantity(hWnd, state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_ADD)
			{
				AddOptionFromInput(hWnd, state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_UPDATE)
			{
				UpdateSelectedOptionFromInput(hWnd, state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_REMOVE)
			{
				RemoveSelectedOption(hWnd, state);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_MOVE_UP)
			{
				MoveSelectedOption(hWnd, state, -1);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_OPTION_MOVE_DOWN)
			{
				MoveSelectedOption(hWnd, state, 1);
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_PROJECT_TO_SUMMARY)
			{
				if (state != nullptr && state->ownerWindow != nullptr)
				{
					ShowSummaryWindow(state->ownerWindow);
				}
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_PROJECT_SAVE_CHANGES)
			{
				if (state != nullptr)
				{
					SaveCurrentProject(hWnd);
				}
				return 0;
			}

			if (LOWORD(wParam) == IDC_EDIT_PRODUCT_SAVE)
			{
				if (state != nullptr && state->showProductSaveButton)
				{
					SaveAndRefresh(hWnd, state);
				}
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

// Öffnet das Bearbeitungsfenster für die Produktbearbeitung.
// Falls das Fenster bereits offen ist, wird es nur fokussiert und wiederhergestellt.
// Gibt true zurück, wenn das Fenster erfolgreich geöffnet wurde (oder bereits offen war).
// Gibt false zurück, wenn ownerWindow nullptr oder productFolderPath leer ist.
bool OpenEditProductWindow(HWND ownerWindow, const std::wstring& productFolderPath, bool showProjectSaveButton, bool showProductSaveButton)
{
	if (ownerWindow == nullptr || productFolderPath.empty())
	{
		return false;
	}

	if (g_hEditWindow != nullptr)
	{
		EditWindowState* state = reinterpret_cast<EditWindowState*>(GetWindowLongPtrW(g_hEditWindow, GWLP_USERDATA));
		if (state != nullptr)
		{
			state->showProjectSaveButton = showProjectSaveButton;
			state->showProductSaveButton = showProductSaveButton;
			if (state->saveProjectButton != nullptr)
			{
				ShowWindow(state->saveProjectButton, showProjectSaveButton ? SW_SHOW : SW_HIDE);
			}
			if (state->saveButton != nullptr)
			{
				ShowWindow(state->saveButton, showProductSaveButton ? SW_SHOW : SW_HIDE);
			}
			if (state->showSummaryButton != nullptr)
			{
				ShowWindow(state->showSummaryButton, ShouldShowSummaryButton(state) ? SW_SHOW : SW_HIDE);
			}

			RefreshOptionList(state);

			RECT clientRect{};
			GetClientRect(g_hEditWindow, &clientRect);
			LayoutEditWindowControls(state, clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
		}

		// Es bleibt immer nur ein Bearbeitungsfenster offen; erneute Aufrufe fokussieren nur.
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
	state->showProjectSaveButton = showProjectSaveButton;
	state->showProductSaveButton = showProductSaveButton;

	g_hEditWindow = CreateWindowExW(
		WS_EX_APPWINDOW,
		kEditWindowClass,
		L"Produkt bearbeiten",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		880,
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
