#include "framework.h"
#include "ProductDeletion.h"
#include "ProductCalculator.h"
#include "AppLocalization.h"

#include <filesystem>

ProductDeletionStatus ConfirmAndDeleteProduct(HWND ownerWindow, const std::wstring& productFolderPath)
{
	if (productFolderPath.empty())
	{
		return ProductDeletionStatus::Failed;
	}

	AppLocalization::SetLanguageFromSettings(g_appSettings);
	const std::filesystem::path folderPath(productFolderPath);
	const std::wstring productName = folderPath.filename().wstring();
	const std::wstring confirmationText =
		AppLocalization::Text(L"delete_product.message.confirm_prefix", L"Möchten Sie das Produkt \"") + productName
		+ AppLocalization::Text(L"delete_product.message.confirm_suffix", L"\" wirklich löschen?\nDieser Vorgang kann nicht rückgängig gemacht werden.");

	const int confirmationResult = MessageBoxW(
		ownerWindow,
		confirmationText.c_str(),
		AppLocalization::Text(L"delete_product.title", L"Produkt löschen").c_str(),
		MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

	if (confirmationResult != IDYES)
	{
		return ProductDeletionStatus::Cancelled;
	}

	std::error_code ec;
	if (!std::filesystem::exists(folderPath, ec) || !std::filesystem::is_directory(folderPath, ec))
	{
		MessageBoxW(ownerWindow, AppLocalization::Text(L"delete_product.message.folder_missing", L"Der Produktordner wurde nicht gefunden.").c_str(), AppLocalization::Text(L"delete_product.title", L"Produkt löschen").c_str(), MB_OK | MB_ICONERROR);
		return ProductDeletionStatus::Failed;
	}

	const std::uintmax_t removedEntries = std::filesystem::remove_all(folderPath, ec);
	if (ec || removedEntries == 0)
	{
		MessageBoxW(ownerWindow, AppLocalization::Text(L"delete_product.message.delete_failed", L"Das Produkt konnte nicht gelöscht werden.").c_str(), AppLocalization::Text(L"delete_product.title", L"Produkt löschen").c_str(), MB_OK | MB_ICONERROR);
		return ProductDeletionStatus::Failed;
	}

	MessageBoxW(ownerWindow, AppLocalization::Text(L"delete_product.message.deleted", L"Produkt wurde gelöscht.").c_str(), AppLocalization::Text(L"delete_product.title", L"Produkt löschen").c_str(), MB_OK | MB_ICONINFORMATION);
	return ProductDeletionStatus::Deleted;
}
