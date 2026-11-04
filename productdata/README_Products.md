# Product Data

Each product is stored in its own subfolder under `productdata/`.

## Creating New Products in the Application

1. Click **New Product** in the application and enter a name.
2. The application automatically creates the product folder and the `product.json` file within it.
3. The file initially contains the product ID, name, article number `"-"`, a base price of `0.0`, and an empty options list.
4. Base price, article number, and options can subsequently be maintained via **Edit Product**.

## Data Structure

`productdata/<productName>/product.json`

The JSON file contains:
- `productId` (Text)
- `productName` (Text)
- `artikelnummer` (Text; if no number, `"-"`)
- `basispreis` (number)
- `optionen` (list with `name`, `artikelnummer`, and `price`; missing option article numbers are treated as `"-"`)

Manual creation or editing of folders and JSON files is still possible, but not required for new products.