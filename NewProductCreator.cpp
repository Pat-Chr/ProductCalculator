#include "framework.h"
#include "NewProductCreator.h"

#include <fstream>
#include <filesystem>
#include <string>

namespace
{
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
}

std::filesystem::path CreateNewProductFolderWithDefaults()
{
	const std::filesystem::path productDataPath = FindProductDataDirectoryForCreation();
	if (productDataPath.empty())
	{
		return {};
	}

	for (int suffix = 1; suffix < 10000; ++suffix)
	{
		const std::wstring folderName = (suffix == 1)
			? L"neuesprodukt"
			: (L"neuesprodukt_" + std::to_wstring(suffix));
		const std::filesystem::path candidatePath = productDataPath / folderName;

		std::error_code existsError;
		if (std::filesystem::exists(candidatePath, existsError))
		{
			continue;
		}

		std::error_code createError;
		if (!std::filesystem::create_directory(candidatePath, createError) || createError)
		{
			return {};
		}

		const std::filesystem::path configPath = candidatePath / L"produkt.json";
		std::ofstream file(configPath, std::ios::binary | std::ios::trunc);
		if (!file)
		{
			std::error_code removeError;
			std::filesystem::remove_all(candidatePath, removeError);
			return {};
		}

		file << "{\n"
			<< "  \"produktId\": \"neuesprodukt\",\n"
			<< "  \"produktname\": \"Neues Produkt\",\n"
			<< "  \"basispreis\": 0.0,\n"
			<< "  \"optionen\": []\n"
			<< "}\n";

		if (!file)
		{
			file.close();
			std::error_code removeError;
			std::filesystem::remove_all(candidatePath, removeError);
			return {};
		}

		return candidatePath;
	}

	return {};
}
