# ProductCalculator

## Overview

ProductCalculator is a Win32 desktop application for configuring products based on JSON files and dynamically calculating the final price.

## Metadata

- **Created by:** Patrick Christe
- **Date:** 01.11.2026
- **License:** MIT License (see [LICENSE.txt](LICENSE.txt))
- **Repository:** [GitHub](https://github.com/Pat-Chr/ProductCalculator)

## Purpose

The application helps quickly assemble suitable product variants, calculate prices transparently, and manage configurations directly.

## Screenshot

![ProductCalculator – Application Overview](docs/images/productcalculator-screenshot.png)

## Features

- Selection of existing products from `productdata/*`
- Dynamic price calculation from base price + options × quantity
- Editing of base price and options in a separate edit window
- Creating new products including default `product.json`
- Deleting existing products with safety confirmation
- Result overview in a separate window with copy function (Clipboard)
- Save/load project as `.pcproj` including quantities and embedded product configuration
- When loading, the saved configuration in the project is preferred (also for custom options)
- Temporarily restored products are automatically cleaned up when closing
- Currency configuration via `settings.json`

## Typical Workflow

1. Select a product from the dropdown.
2. Enter quantities per option (default value per option: `0`).
3. Check that the final price is updated live.
4. Optionally edit, create new, or delete a product.
5. Open the overview via "Calculate" and copy it.
6. Optionally save or load a project as `.pcproj`.

## Data Format

Product data for each product is stored in its own folder:
- `productdata/<productName>/product.json`

Example structure:
- `productId` (String)
- `productName` (String)
- `basispreis` (number)
- `optionen[]` with `name` and `price`

App settings:
- `settings.json` → `standardwaehrung` (e.g., `CHF`)

## Technical Notes

- Project type: C++ Windows desktop application (Win32 API)
- Language: C++20
- UI: German
- Persistence: JSON files (UTF-8)
- No external framework dependencies required

## Build and Execution

1. Open the project in Visual Studio.
2. Select a configuration (e.g., `Debug | x64`).
3. Build and run the project.

Note:
- Post-build copies `README.md`, `LICENSE.txt`, `settings.json`, and the `productdata` folder to the output directory.

## Project Structure (brief)

- `ProductCalculator.cpp/.h` – Main window, selection, price logic, and project actions
- `EditProductWindow.cpp/.h` – Product editing
- `NewProductCreator.cpp/.h` – Creating new products
- `ProductCopyCreator.cpp/.h` – Product copy
- `ProductDeletion.cpp/.h` – Product deletion
- `ProductSummaryWindow.cpp` – Result window
- `ProjectPersistence.cpp/.h` – Loading/saving `.pcproj`
- `RestoredProductsCleanup.cpp/.h` – Cleanup of temporary restorations on exit
- `ProductCalculator.rc` / `Resource.h` – Resources and IDs
- `productdata/` – Product catalog
- `settings.json` – Default settings
- `ARCHITECTURE.md` – Technical architecture overview

## Current Status

- ✅ Product selection and live price calculation
- ✅ Product editing with synchronization to main window
- ✅ Creating new, copying, and deleting products
- ✅ Result overview with Clipboard copy
- ✅ Save/load project (`.pcproj`) including embedded product configuration
- ✅ Cleanup of `saved_projects/restored_products` when program ends
- ✅ Currency control via settings

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) – detailed architecture
- [CHANGELOG.md](CHANGELOG.md) – changes per version
- [productdata/README_Products.md](productdata/README_Products.md) – product data structure
- [LICENSE.txt](LICENSE.txt) – license
