#include "framework.h"
#include "RestoredProductsCleanup.h"
#include "ProjectPersistence.h"

#include <filesystem>

bool ClearRestoredProductsDirectory()
{
	const std::filesystem::path restoredRoot = EnsureSavedProjectsDirectory() / L"restored_products";

	std::error_code ec;
	std::filesystem::remove_all(restoredRoot, ec);
	if (ec)
	{
		return false;
	}

	std::filesystem::create_directories(restoredRoot, ec);
	return !ec;
}
