#pragma once

#include <filesystem>
#include <windows.h>

inline std::filesystem::path FindOrMigrateProductDataDirectory()
{
	wchar_t modulePath[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, modulePath, MAX_PATH);

	std::filesystem::path currentPath = std::filesystem::path(modulePath).parent_path();
	while (!currentPath.empty())
	{
		const std::filesystem::path preferredPath = currentPath / L"productdata";
		const std::filesystem::path legacyPath = currentPath / L"produktdaten";
		std::error_code ec;

		if (std::filesystem::exists(preferredPath, ec) && std::filesystem::is_directory(preferredPath, ec))
		{
			return preferredPath;
		}

		ec.clear();
		if (std::filesystem::exists(legacyPath, ec) && std::filesystem::is_directory(legacyPath, ec))
		{
			ec.clear();
			std::filesystem::rename(legacyPath, preferredPath, ec);
			if (!ec)
			{
				return preferredPath;
			}

			return legacyPath;
		}

		if (currentPath == currentPath.root_path())
		{
			break;
		}

		currentPath = currentPath.parent_path();
	}

	return {};
}
