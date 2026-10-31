#include "framework.h"
#include "ProductDeletion.h"

#include <filesystem>

ProductDeletionStatus ConfirmAndDeleteProduct(HWND ownerWindow, const std::wstring& productFolderPath)
{
	if (productFolderPath.empty())
	{
		return ProductDeletionStatus::Failed;
	}

	const std::filesystem::path folderPath(productFolderPath);
	const std::wstring productName = folderPath.filename().wstring();
	const std::wstring confirmationText =
		L"Möchten Sie das Produkt \"" + productName + L"\" wirklich löschen?\nDieser Vorgang kann nicht rückgängig gemacht werden.";

	const int confirmationResult = MessageBoxW(
		ownerWindow,
		confirmationText.c_str(),
		L"Produkt löschen",
		MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);

	if (confirmationResult != IDYES)
	{
		return ProductDeletionStatus::Cancelled;
	}

	std::error_code ec;
	if (!std::filesystem::exists(folderPath, ec) || !std::filesystem::is_directory(folderPath, ec))
	{
		MessageBoxW(ownerWindow, L"Der Produktordner wurde nicht gefunden.", L"Produkt löschen", MB_OK | MB_ICONERROR);
		return ProductDeletionStatus::Failed;
	}

	const std::uintmax_t removedEntries = std::filesystem::remove_all(folderPath, ec);
	if (ec || removedEntries == 0)
	{
		MessageBoxW(ownerWindow, L"Das Produkt konnte nicht gelöscht werden.", L"Produkt löschen", MB_OK | MB_ICONERROR);
		return ProductDeletionStatus::Failed;
	}

	MessageBoxW(ownerWindow, L"Produkt wurde gelöscht.", L"Produkt löschen", MB_OK | MB_ICONINFORMATION);
	return ProductDeletionStatus::Deleted;
}
