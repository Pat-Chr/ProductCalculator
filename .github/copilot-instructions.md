# Copilot Instructions

## General Guidelines
- Beim Speichern im Produkt-Bearbeitungsfenster soll das Fenster offen bleiben; es soll nur aktualisiert werden.
- Im ProductCalculator soll die Mengen-Eingabe pro Option standardmäßig 0 sein (nicht 1).
- Button-Beschriftungen für Ergebnisanzeige und Summary sollen einheitlich 'Kalkulieren' heißen.
- Design- und Formatentscheidungen sollen immer in den Copilot-Anweisungen (.github/copilot-instructions.md) dokumentiert werden.

## Code Structure
- Trenne neue Features in eigene .cpp/.h Module für bessere Erweiterbarkeit, anstatt Logik direkt in ProductCalculator.cpp zu implementieren.