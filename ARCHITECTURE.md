# ARCHITECTURE.md - ProductCalculator


## 1. Overview

### 1.1 Short Description
ProductCalculator is a Win32 desktop application for configuring products based on JSON files and dynamically calculating the final price.

### 1.2 Objective
The application should make product configurations easy to select, edit, create anew, and delete, including immediate price updates and exportable result overview.

### 1.3 Use Case
- Simple quotation and calculation scenarios
- Standard products with variable options and quantities
- Windows desktop environment (Win32 API, C++20)


---

## 2. System Architecture

### 2.1 Main Components

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              ProductCalculator                               │
│                                                                              │
│  ┌─────────────────────┐      ┌─────────────────────┐      ┌─────────────-┐  │
│  │       UI Layer      │      │      Data Layer     │      │ Logic Layer  │  │
│  │ - Main Window       │      │ - settings.json     │      │ - Price Form.│  │
│  │ - Edit Window       │      │ - productdata/*     │      │ - Sums       │  │
│  │ - New Product Dialog│      │ - product.json      │      │ - Validation │  │
│  │ - Summary Window    │      │ - UTF-8 JSON I/O    │      │ - UI Sync    │  │
│  └─────────────────────┘      └─────────────────────┘      └─────────────-┘  │
│                                                                              │
│  ┌─────────────────────┐      ┌─────────────────────┐                        │
│  │   Resource Layer    │      │    System Layer     │                        │
│  │ - Menus/Dialogs     │      │ - Win32 API         │                        │
│  │ - IDs in Resource.h │      │ - GDI / MessageLoop │                        │
│  │ - Icons/Strings     │      │ - std::filesystem   │                        │
│  └─────────────────────┘      └─────────────────────┘                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Data Flow

```
User Action
  ↓
WndProc / Window Handler
  ↓
Load/Save Product JSON / Project File
  ↓
Price Calculation + UI Rebuild
  ↓
Display (Labels, Quantity Fields, Summary, MessageBox)
```

### 2.3 Cross-Window Synchronization
- The edit window sends `WM_PRODUCT_CONFIGURATION_UPDATED` to the main window.
- The main window then reloads the dropdown and configuration.
- Saving in the edit window does not close it, but only updates the state.


---

## 3. Modules and Files

### 3.1 Main Modules

| File | Purpose |
|-------|-------|
| `ProductCalculator.cpp` | Entry point, main window, layout, dropdown population, price calculation |
| `ProductCalculator.h` | Shared data types and global interfaces |
| `EditProductWindow.cpp/.h` | Separate edit window for base price/options including JSON persistence |
| `NewProductCreator.cpp/.h` | Dialog and creation of new product folders with default `product.json` |
| `ProductDeletion.cpp/.h` | Confirmation and recursive deletion of a product folder |
| `ProductSummaryWindow.cpp` | Separate window for text summary and clipboard copy |
| `ProjectPersistence.cpp/.h` | Save/Load project files (`.pcproj`) including quantities and embedded product JSON |
| `RestoredProductsCleanup.cpp/.h` | Cleanup of `saved_projects/restored_products` on program exit |
| `ProductCalculator.rc` | Resources (menus, dialogs, icons, string table) |
| `Resource.h` | Resource IDs (buttons, dialogs, controls, menus) |

### 3.2 Build and Runtime Files

| File | Purpose |
|-------|-------|
| `ProductCalculator.vcxproj` | Project definition (C++20, sources, post-build copy) |
| `settings.json` | App setting `standardcurrency` |
| `productdata/<product>/product.json` | Persistent product definition |
| `saved_projects/*.pcproj` | Persistent project states (product reference, quantities, embedded product JSON) |
| `saved_projects/restored_products/*` | Runtime restorations from saved projects (emptied on exit) |

### 3.3 Central Global States

```cpp
HINSTANCE hInst;
WCHAR szTitle[MAX_LOADSTRING];
WCHAR szWindowClass[MAX_LOADSTRING];
HWND g_hProductDropdown;
std::vector<HWND> g_optionQuantityInputs;
std::vector<std::wstring> g_productFolderPaths;
std::wstring g_selectedProductPath;
ProductConfiguration g_currentConfiguration;
AppSettings g_appSettings;
```


---

## 4. Data Model

### 4.1 Product Model (`product.json`)

```json
{
  "productId": "exampleproduct",
  "productName": "Example Product",
  "basePrice": 1000.0,
  "options": [
    { "name": "Standard Equipment", "price": 0.0 },
    { "name": "Premium Package", "price": 249.9 }
  ]
}
```

### 4.2 Internal Structures
- `ProductOption { name, price }`
- `ProductConfiguration { basePrice, options[] }`
- `AppSettings { currency }`

### 4.3 Price Calculation

```
FinalPrice = basePrice + Σ(option.price × quantity)
```

Notes:
- Quantity fields are dynamically generated per option.
- Initial quantity value is `0`.
- Negative inputs are clamped to `0`.


---

## 5. Design Decisions

### 5.1 Win32 + STL instead of External Frameworks
- No external runtime dependencies
- Direct access to Message Loop and Control behavior
- Good portability within classic Windows desktop environments

### 5.2 Modular Split by Feature
- Product editing, creation, deletion, and summary are encapsulated in separate modules.
- Aligns with the goal of not mixing new features directly into `ProductCalculator.cpp`.

### 5.3 File-Based Persistence
- Product data resides in its own subfolder per product.
- `productdata` and `settings.json` are copied to the output directory via post-build.
- Paths are robustly searched relative to EXE location in parent directories.

### 5.4 JSON Processing
- JSON is processed without third-party library via string/regex processing.
- UTF-8 ↔ Wide-String conversion is centrally considered.


---

## 6. Main Workflows

### 6.1 Start and Initialization
1. `wWinMain` loads resource strings.
2. `settings.json` is read (`currency`).
3. Main window is created.
4. Product dropdown is populated with folders under `productdata`.

### 6.2 Product Selection and Calculation
1. User selects product in dropdown.
2. `product.json` is loaded.
3. Options + quantity fields are re-rendered.
4. On quantity change, final price is recalculated immediately.

### 6.3 Edit Product
1. Open via menu or button.
2. If window already exists: only focus/restore.
3. Save writes `product.json`.
4. Main window is synchronized via `WM_PRODUCT_CONFIGURATION_UPDATED`.
5. Edit window remains open.

### 6.4 Create New Product
1. Name is queried via dialog.
2. Folder name is cleaned and checked for collision.
3. Folder + default `product.json` (`basePrice: 0.0`, `options: []`) are created.
4. Dropdown is updated and new product selected.

### 6.5 Delete Product
1. Safety prompt via `MessageBox`.
2. Recursive deletion of selected product folder.
3. Dropdown and display state are updated.

### 6.6 Result Overview
- Separate window class shows base price, options, quantities, line sums, and total price.
- Text can be copied directly to clipboard.

### 6.7 Save/Load Project
1. Save creates a `.pcproj` with product path/name, quantities, and embedded `product.json`.
2. Load restores the embedded product configuration preferably.
3. If no embedded product exists, fallback via product path/name occurs.
4. After loading, quantity fields are populated and calculation updated.

### 6.8 Exit and Cleanup
1. On `WM_DESTROY`, dynamic controls and states are released.
2. `saved_projects/restored_products` is fully cleaned.


---

## 7. Interfaces and Extensibility

### 7.1 Important Public Functions

| Function | Description |
|----------|--------------|
| `OpenEditProductWindow(HWND, const std::wstring&)` | Opens/focuses product editing |
| `CreateNewProductFolderWithDefaults(HWND, std::filesystem::path&)` | Creates new product with default file |
| `ConfirmAndDeleteProduct(HWND, const std::wstring&)` | Confirms and deletes product folder |
| `ShowSummaryWindow(HWND)` | Opens result overview |
| `SaveCurrentProject(HWND)` | Saves current state as `.pcproj` |
| `LoadProjectIntoCalculator(HWND, HWND)` | Loads saved project state into calculator |
| `FormatPrice(double)` | Price formatting including currency |

### 7.2 Extension Points
- Additional pricing rules (discounts, tiered prices, taxes)
- Validation schemas for `product.json`
- Multi-language UI strings via resources
- Persistent sort/filter in product dropdown


---

## 8. Status and Roadmap

### 8.1 Current Status
- ✅ Main window with product selection exists
- ✅ Dynamic price calculation with quantity fields exists
- ✅ Product editing (base price + options) exists
- ✅ New product creation exists
- ✅ Copying products exists
- ✅ Deleting products exists
- ✅ Result overview with copy function exists
- ✅ Project save/load (`.pcproj`) exists
- ✅ Restored runtime data is cleaned on exit
- ✅ Settings (currency) via `settings.json` exists

### 8.2 Possible Next Steps
1. Introduce robust JSON parser library instead of regex
2. Add automated tests for parser/price logic
3. Expand error diagnostics (e.g., detailed file errors)
4. Optionally add import/export functions for product catalogs


---

## 9. Conventions

### 9.1 Naming
- Resource IDs: `IDM_`, `IDD_`, `IDC_`, `IDI_`
- Global handles/states: `g_` prefix
- Win32 entry points and WndProc follow WinAPI convention

### 9.2 UI/Behavior Conventions
- Product edit window remains open after save.
- Quantity fields start with `0` by default.
- Button label for result display/summary is consistently `Calculate`.

### 9.3 Technical Conventions
- Language standard: C++20
- Character encoding: UTF-8 in files, Wide-Strings in UI context


---

## 10. References

- [README.md](README.md) – Project overview
- [productdata/README_Produkte.md](productdata/README_Produkte.md) – Product data structure
- [LICENSE.txt](LICENSE.txt) – MIT License
- Windows SDK documentation for Win32 API


---

*Last change: 01.11.2026*  
*Author: Patrick Christe*
