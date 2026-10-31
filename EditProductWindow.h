#pragma once

#include <string>
#include <windows.h>

// Ereigniskonstante für Produktkonfigurations-Updates.
// Wird vom Bearbeitungsfenster an das Hauptfenster gesendet, um eine Synchronisation auszulösen.
constexpr UINT WM_PRODUCT_CONFIGURATION_UPDATED = WM_APP + 1;

// Öffnet das Bearbeitungsfenster für die Produktbearbeitung.
// Falls das Fenster bereits offen ist, wird es nur fokussiert und wiederhergestellt.
// Parameter:
//   ownerWindow - Das Hauptfenster (für Modality und Nachrichten)
//   productFolderPath - Pfad zum Produktdaten-Verzeichnis mit der produkt.json Datei
// Rückgabewert:
//   true - Wenn das Fenster erfolgreich geöffnet wurde (oder bereits offen war)
//   false - Wenn ownerWindow nullptr oder productFolderPath leer ist
bool OpenEditProductWindow(HWND ownerWindow, const std::wstring& productFolderPath);
