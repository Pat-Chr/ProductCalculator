# ARCHITECTURE.md - ProductCalculator

## 1. Überblick

### 1.1 Kurze Beschreibung
ProductCalculator ist eine Win32-Desktopanwendung zur Konfiguration von Produkten auf Basis von JSON-Dateien und zur dynamischen Berechnung des Endpreises.

### 1.2 Zielsetzung
Die Anwendung soll Produktkonfigurationen einfach auswählbar, bearbeitbar, neu anlegbar und löschbar machen, inklusive sofortiger Preisaktualisierung und exportierbarer Ergebnisübersicht.

### 1.3 Einsatzgebiet
- Einfache Angebots- und Kalkulationsszenarien
- Standardprodukte mit variablen Optionen und Mengen
- Windows-Desktopumgebung (Win32 API, C++20)

---

## 2. Systemarchitektur

### 2.1 Hauptkomponenten

```
┌──────────────────────────────────────────────────────────────────────────────┐
│                              ProductCalculator                               │
│                                                                              │
│  ┌─────────────────────┐      ┌─────────────────────┐      ┌─────────────-┐  │
│  │       UI Layer      │      │      Data Layer     │      │ Logic Layer  │  │
│  │ - Hauptfenster      │      │ - settings.json     │      │ - Preisform. │  │
│  │ - Edit-Fenster      │      │ - produktdaten/*    │      │ - Summen     │  │
│  │ - Neu-Produkt-Dialog│      │ - produkt.json      │      │ - Validierung│  │
│  │ - Summary-Fenster   │      │ - UTF-8 JSON I/O    │      │ - UI Sync    │  │
│  └─────────────────────┘      └─────────────────────┘      └─────────────-┘  │
│                                                                              │
│  ┌─────────────────────┐      ┌─────────────────────┐                        │
│  │   Resource Layer    │      │    System Layer     │                        │
│  │ - Menüs/Dialoge     │      │ - Win32 API         │                        │
│  │ - IDs in Resource.h │      │ - GDI / MessageLoop │                        │
│  │ - Icons/Strings     │      │ - std::filesystem   │                        │
│  └─────────────────────┘      └─────────────────────┘                        │
└──────────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Datenfluss

```
Benutzeraktion
   ↓
WndProc / Fenster-Handler
   ↓
Produkt-JSON / Projektdatei laden/speichern
   ↓
Preisberechnung + UI-Rebuild
   ↓
Anzeige (Labels, Mengenfelder, Summary, MessageBox)
```

### 2.3 Fensterübergreifende Synchronisation
- Das Bearbeitungsfenster sendet `WM_PRODUCT_CONFIGURATION_UPDATED` an das Hauptfenster.
- Das Hauptfenster lädt daraufhin Dropdown und Konfiguration neu.
- Speichern im Bearbeitungsfenster schließt das Fenster nicht, sondern aktualisiert nur den Zustand.

---

## 3. Module und Dateien

### 3.1 Hauptmodule

| Datei | Zweck |
|-------|-------|
| `ProductCalculator.cpp` | Einstiegspunkt, Hauptfenster, Layout, Dropdown-Befüllung, Preisberechnung |
| `ProductCalculator.h` | Gemeinsame Datentypen und globale Schnittstellen |
| `EditProductWindow.cpp/.h` | Eigenes Bearbeitungsfenster für Basispreis/Optionen inkl. JSON-Speicherung |
| `NewProductCreator.cpp/.h` | Dialog und Erzeugung neuer Produktordner mit Default-`produkt.json` |
| `ProductDeletion.cpp/.h` | Bestätigung und rekursives Löschen eines Produktordners |
| `ProductSummaryWindow.cpp` | Eigenes Fenster für Textzusammenfassung und Clipboard-Kopie |
| `ProjectPersistence.cpp/.h` | Speichern/Laden von Projektdateien (`.pcproj`) inkl. Mengen und eingebettetem Produkt-JSON |
| `RestoredProductsCleanup.cpp/.h` | Bereinigung von `saved_projects/restored_products` beim Programmende |
| `ProductCalculator.rc` | Ressourcen (Menü, Dialoge, Icons, Stringtable) |
| `Resource.h` | Ressourcen-IDs (Buttons, Dialoge, Controls, Menüs) |

### 3.2 Build- und Laufzeitdateien

| Datei | Zweck |
|-------|-------|
| `ProductCalculator.vcxproj` | Projektdefinition (C++20, Quellen, Post-Build-Copy) |
| `settings.json` | App-Einstellung `standardwaehrung` |
| `produktdaten/<produkt>/produkt.json` | Persistente Produktdefinition |
| `saved_projects/*.pcproj` | Persistente Projektstände (Produktbezug, Mengen, eingebettetes Produkt-JSON) |
| `saved_projects/restored_products/*` | Laufzeit-Wiederherstellungen aus gespeicherten Projekten (wird beim Beenden geleert) |

### 3.3 Zentrale globale Zustände

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

## 4. Datenmodell

### 4.1 Produktmodell (`produkt.json`)

```json
{
  "produktId": "beispielprodukt",
  "produktname": "Beispielprodukt",
  "basispreis": 1000.0,
  "optionen": [
    { "name": "Standardausstattung", "preis": 0.0 },
    { "name": "Premium-Paket", "preis": 249.9 }
  ]
}
```

### 4.2 Interne Strukturen
- `ProductOption { name, price }`
- `ProductConfiguration { basePrice, options[] }`
- `AppSettings { standardwaehrung }`

### 4.3 Preisberechnung

```
Endpreis = basispreis + Σ(option.preis × menge)
```

Hinweise:
- Mengenfelder werden pro Option dynamisch erzeugt.
- Startwert der Mengen ist `0`.
- Negative Eingaben werden auf `0` begrenzt.

---

## 5. Design-Entscheidungen

### 5.1 Win32 + STL statt externer Frameworks
- Keine externen Runtime-Abhängigkeiten
- Direkter Zugriff auf Message-Loop und Control-Verhalten
- Gute Portabilität innerhalb klassischer Windows-Desktopumgebungen

### 5.2 Modulare Aufteilung nach Feature
- Produktbearbeitung, Neuerstellung, Löschung und Summary sind in separaten Modulen gekapselt.
- Entspricht dem Ziel, neue Features nicht direkt in `ProductCalculator.cpp` zu vermischen.

### 5.3 Dateibasierte Persistenz
- Produktdaten liegen je Produkt in eigenem Unterordner.
- `produktdaten` und `settings.json` werden per Post-Build in das Ausgabeverzeichnis kopiert.
- Pfade werden robust relativ zum EXE-Standort in Parent-Verzeichnissen gesucht.

### 5.4 JSON-Verarbeitung
- JSON wird ohne Drittbibliothek per String-/Regex-Verarbeitung verarbeitet.
- UTF-8 ↔ Wide-String Konvertierung ist zentral berücksichtigt.

---

## 6. Hauptabläufe

### 6.1 Start und Initialisierung
1. `wWinMain` lädt Ressourcen-Strings.
2. `settings.json` wird gelesen (`standardwaehrung`).
3. Hauptfenster wird erstellt.
4. Produkt-Dropdown wird mit Ordnern unter `produktdaten` gefüllt.

### 6.2 Produktauswahl und Kalkulation
1. Benutzer wählt Produkt im Dropdown.
2. `produkt.json` wird geladen.
3. Optionen + Mengenfelder werden neu gerendert.
4. Bei Mengenänderung wird der Endpreis sofort neu berechnet.

### 6.3 Produkt bearbeiten
1. Öffnen über Menü oder Button.
2. Falls Fenster bereits existiert: nur fokussieren/wiederherstellen.
3. Speichern schreibt `produkt.json`.
4. Hauptfenster wird via `WM_PRODUCT_CONFIGURATION_UPDATED` synchronisiert.
5. Bearbeitungsfenster bleibt geöffnet.

### 6.4 Neues Produkt anlegen
1. Name wird per Dialog abgefragt.
2. Ordnername wird bereinigt und auf Kollision geprüft.
3. Ordner + Default-`produkt.json` (`basispreis: 0.0`, `optionen: []`) werden erzeugt.
4. Dropdown wird aktualisiert und neues Produkt selektiert.

### 6.5 Produkt löschen
1. Sicherheitsabfrage per `MessageBox`.
2. Rekursives Löschen des gewählten Produktordners.
3. Dropdown und Anzeigezustand werden aktualisiert.

### 6.6 Ergebnisübersicht
- Separate Fensterklasse zeigt Basispreis, Optionen, Mengen, Zeilensummen und Gesamtpreis.
- Text kann direkt in die Zwischenablage kopiert werden.

### 6.7 Projekt speichern/laden
1. Speichern erzeugt eine `.pcproj` mit Produktpfad/-name, Mengen und eingebettetem `produkt.json`.
2. Laden stellt bevorzugt die eingebettete Produktkonfiguration wieder her.
3. Falls kein eingebettetes Produkt vorhanden ist, erfolgt Fallback über Produktpfad/-name.
4. Nach dem Laden werden Mengenfelder befüllt und die Kalkulation aktualisiert.

### 6.8 Beenden und Aufräumen
1. Beim `WM_DESTROY` werden dynamische Controls und Zustände freigegeben.
2. `saved_projects/restored_products` wird vollständig bereinigt.

---

## 7. Schnittstellen und Erweiterbarkeit

### 7.1 Wichtige öffentliche Funktionen
| Funktion | Beschreibung |
|----------|--------------|
| `OpenEditProductWindow(HWND, const std::wstring&)` | Öffnet/fokussiert Produktbearbeitung |
| `CreateNewProductFolderWithDefaults(HWND, std::filesystem::path&)` | Legt neues Produkt mit Standarddatei an |
| `ConfirmAndDeleteProduct(HWND, const std::wstring&)` | Bestätigt und löscht Produktordner |
| `ShowSummaryWindow(HWND)` | Öffnet Ergebnisübersicht |
| `SaveCurrentProject(HWND)` | Speichert aktuellen Stand als `.pcproj` |
| `LoadProjectIntoCalculator(HWND, HWND)` | Lädt einen gespeicherten Projektstand in die Kalkulation |
| `FormatPrice(double)` | Preisformatierung inkl. Währung |

### 7.2 Erweiterungspunkte
- Zusätzliche Preisregeln (Rabatte, Staffelpreise, Steuern)
- Validierungsschemata für `produkt.json`
- Mehrsprachige UI-Strings über Ressourcen
- Persistente Sortierung/Filter im Produkt-Dropdown

---

## 8. Status und Roadmap

### 8.1 Aktueller Status
- ✅ Hauptfenster mit Produktauswahl vorhanden
- ✅ Dynamische Preisberechnung mit Mengenfeldern vorhanden
- ✅ Produktbearbeitung (Basispreis + Optionen) vorhanden
- ✅ Neuanlage von Produkten vorhanden
- ✅ Kopieren von Produkten vorhanden
- ✅ Löschen von Produkten vorhanden
- ✅ Ergebnisübersicht mit Kopierfunktion vorhanden
- ✅ Projekt speichern/laden (`.pcproj`) vorhanden
- ✅ Wiederhergestellte Laufzeitdaten werden beim Beenden bereinigt
- ✅ Einstellungen (Währung) über `settings.json` vorhanden

### 8.2 Mögliche nächste Schritte
1. Robuste JSON-Parser-Bibliothek statt Regex einführen
2. Automatisierte Tests für Parser/Preislogik ergänzen
3. Fehlerdiagnostik (z. B. detaillierte Dateifehler) erweitern
4. Optionale Import/Export-Funktionen für Produktkataloge ergänzen

---

## 9. Konventionen

### 9.1 Benennung
- Ressourcen-IDs: `IDM_`, `IDD_`, `IDC_`, `IDI_`
- Globale Handles/Zustände: `g_`-Präfix
- Win32-Einstiegspunkte und WndProc nach WinAPI-Konvention

### 9.2 UI-/Verhaltenskonventionen
- Produkt-Bearbeitungsfenster bleibt nach Speichern offen.
- Mengenfelder starten standardmäßig mit `0`.
- Button-Beschriftung für Ergebnisanzeige/Summary ist einheitlich `Kalkulieren`.

### 9.3 Technische Konventionen
- Sprachstandard: C++20
- Zeichenkodierung: UTF-8 in Dateien, Wide-Strings im UI-Kontext

---

## 10. Referenzen

- [README.md](README.md) – Projektüberblick
- [produktdaten/README_Produkte.md](produktdaten/README_Produkte.md) – Produktdatenstruktur
- [LICENSE.txt](LICENSE.txt) – MIT License
- Windows SDK Dokumentation für Win32 API

---

*Letzte Änderung: 01.11.2026*  
*Autor: Patrick Christe*
