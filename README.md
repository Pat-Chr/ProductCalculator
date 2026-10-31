# ProductCalculator

## Überblick
ProductCalculator ist eine Win32-Desktopanwendung zur Konfiguration von Produkten auf Basis von JSON-Dateien und zur dynamischen Berechnung des Endpreises.

## Metadaten
- **Erstellt von:** Patrick Christe
- **Datum:** 31.10.2026
- **Lizenz:** MIT License (siehe [LICENSE.txt](LICENSE.txt))
- **Repository:** [GitHub](https://github.com/Pat-Chr/ProductCalculator)

## Zweck
Die Anwendung unterstützt dabei, schnell passende Produktvarianten zusammenzustellen, Preise transparent zu berechnen und Konfigurationen direkt zu pflegen.

## Funktionsumfang
- Auswahl vorhandener Produkte aus `produktdaten/*`
- Dynamische Preisberechnung aus Basispreis + Optionen × Menge
- Bearbeiten von Basispreis und Optionen in eigenem Bearbeitungsfenster
- Anlegen neuer Produkte inkl. Standard-`produkt.json`
- Löschen bestehender Produkte mit Sicherheitsabfrage
- Ergebnisübersicht in eigenem Fenster mit Kopierfunktion (Clipboard)
- Währungskonfiguration über `settings.json`

## Typischer Ablauf
1. Produkt im Dropdown auswählen.
2. Mengen je Option eingeben (Startwert je Option: `0`).
3. Endpreis live aktualisiert prüfen.
4. Optional Produkt bearbeiten, neu anlegen oder löschen.
5. Ergebnis über „Ergebnis anzeigen“ als Übersicht öffnen und kopieren.

## Datenformat
Produktdaten liegen je Produkt in einem eigenen Ordner:
- `produktdaten/<produktname>/produkt.json`

Beispielstruktur:
- `produktId` (String)
- `produktname` (String)
- `basispreis` (Zahl)
- `optionen[]` mit `name` und `preis`

App-Einstellungen:
- `settings.json` → `standardwaehrung` (z. B. `CHF`)

## Technische Hinweise
- Projekttyp: C++ Windows-Desktopanwendung (Win32 API)
- Sprache: C++20
- Oberfläche: Deutsch
- Persistenz: JSON-Dateien (UTF-8)
- Keine externen Framework-Abhängigkeiten erforderlich

## Build und Ausführung
1. Projekt in Visual Studio öffnen.
2. Konfiguration wählen (z. B. `Debug | x64`).
3. Projekt erstellen und starten.

Hinweis:
- Post-Build kopiert `README.md`, `LICENSE.txt`, `settings.json` und den Ordner `produktdaten` in das Ausgabeverzeichnis.

## Projektstruktur (kurz)
- `ProductCalculator.cpp/.h` – Hauptfenster, Auswahl, Preislogik
- `EditProductWindow.cpp/.h` – Produktbearbeitung
- `NewProductCreator.cpp/.h` – Neuanlage von Produkten
- `ProductDeletion.cpp/.h` – Produktlöschung
- `ProductSummaryWindow.cpp` – Ergebnisfenster
- `ProductCalculator.rc` / `Resource.h` – Ressourcen und IDs
- `produktdaten/` – Produktkatalog
- `settings.json` – Grundeinstellungen
- `ARCHITECTURE.md` – technische Architekturübersicht

## Aktueller Status
- ✅ Produktauswahl und live Preisberechnung
- ✅ Produktbearbeitung mit Synchronisation ins Hauptfenster
- ✅ Neuanlage und Löschung von Produkten
- ✅ Ergebnisübersicht mit Clipboard-Kopie
- ✅ Währungssteuerung über Einstellungen

## Dokumentation
- [ARCHITECTURE.md](ARCHITECTURE.md) – detaillierte Architektur
- [produktdaten/README_Produkte.md](produktdaten/README_Produkte.md) – Produktdatenstruktur
- [LICENSE.txt](LICENSE.txt) – Lizenz
