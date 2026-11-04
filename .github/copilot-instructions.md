# Copilot Instructions

## General Guidelines
- Beim Speichern im Produkt-Bearbeitungsfenster soll das Fenster offen bleiben; es soll nur aktualisiert werden.
- Im ProductCalculator soll die Mengen-Eingabe pro Option standardmäßig 0 sein (nicht 1).
- Button-Beschriftungen für Ergebnisanzeige und Summary sollen einheitlich 'Kalkulieren' heißen.
- Design- und Formatentscheidungen sollen immer in den Copilot-Anweisungen (.github/copilot-instructions.md) dokumentiert werden.
- Produkt- und Optionsdaten unterstützen `artikelnummer`; fehlende oder leere Werte werden als `"-"` gespeichert.
- `settings.json` soll erweiterbar bleiben: unbekannte Top-Level-Einträge werden beim Speichern erhalten, damit neue Optionen später ergänzt werden können.
- Einstellungen werden über ein eigenes Einstellungsfenster (`SettingsWindow`) bearbeitet, damit später zusätzliche Optionen zentral erweitert werden können.

## Code Structure
- Trenne neue Features in eigene .cpp/.h Module für bessere Erweiterbarkeit, anstatt Logik direkt in ProductCalculator.cpp zu implementieren.