# ProductCalculator

## Überblick
ProductCalculator ist eine Windows-Anwendung zur Konfiguration von Standardprodukten und zur Berechnung des Verkaufspreises.

## Metadaten
- **Erstellt von:** Patrick Christe
- **Datum:** 31.10.2026
- **Lizenz:** MIT License (siehe [LICENSE.txt](LICENSE.txt))

## Zweck
Das Programm unterstützt dabei, für ein ausgewähltes Standardprodukt schnell eine passende Produktvariante zusammenzustellen und den resultierenden Verkaufspreis auszugeben.

## Funktionsumfang
- Auswahl eines Standardprodukts aus einer Produktliste
- Festlegung der enthaltenen Komponenten pro Produkt
- Automatische Ermittlung und Ausgabe des Verkaufspreises

## Typischer Ablauf
1. Ein Standardprodukt aus der Liste auswählen.
2. Die gewünschten Komponenten für dieses Produkt festlegen.
3. Den berechneten Verkaufspreis anzeigen lassen.

## Einsatzgebiet
Die Anwendung eignet sich für einfache Angebots- und Kalkulationsszenarien, in denen Standardprodukte mit variablen Komponenten verkauft werden.

## Technische Hinweise
- Projekttyp: C++ Windows-Desktopanwendung (Visual Studio)
- Aktuelle Sprache der Oberfläche: Deutsch
- Repository: [GitHub](https://github.com/Pat-Chr/ProductCalculator)

## Build und Ausführung
1. Lösung in Visual Studio öffnen.
2. Gewünschte Konfiguration wählen (z. B. `Debug | x64`).
3. Projekt erstellen und starten.

## Projektstruktur (kurz)
- `ProductCalculator.cpp` – Einstiegspunkt und Fensterlogik
- `ProductCalculator.rc` – Ressourcen (Menü, Dialoge, Strings)
- `ProductCalculator.vcxproj` – Projektkonfiguration
- `LICENSE.txt` – Lizenztext

## Status
Das Projekt befindet sich in aktiver Entwicklung und wird schrittweise erweitert.
