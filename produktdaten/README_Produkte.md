# Produktdaten

Jedes Produkt liegt in einem eigenen Unterordner unter `produktdaten/`.

## Neue Produkte in der Anwendung anlegen

1. In der Anwendung auf **Neues Produkt** klicken und einen Namen eingeben.
2. Die Anwendung erstellt automatisch den Produktordner und darin die Datei `produkt.json`.
3. Die Datei enthält zunächst die Produkt-ID und den Namen, einen Basispreis von `0.0` und eine leere Optionsliste.
4. Basispreis und Optionen können anschließend über **Produkt bearbeiten** gepflegt werden.

## Datenstruktur

`produktdaten/<produktname>/produkt.json`

Die JSON-Datei enthält:
- `produktId` (Text)
- `produktname` (Text)
- `basispreis` (Zahl)
- `optionen` (Liste mit `name` und `preis`)

Eine manuelle Erstellung oder Bearbeitung der Ordner und JSON-Dateien ist weiterhin möglich, aber für neue Produkte nicht erforderlich.
