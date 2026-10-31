# ARCHITECTURE.md - ProductCalculator

## 1. Überblick

### 1.1 Kurze Beschreibung
ProductCalculator ist eine Windows-Desktopanwendung zur Konfiguration von Standardprodukten und zur Berechnung des Verkaufspreises.

### 1.2 Zielsetzung
Das Programm unterstützt dabei, für ein ausgewähltes Standardprodukt schnell eine passende Produktvariante zusammenzustellen und den resultierenden Verkaufspreis auszugeben.

### 1.3 Einsatzgebiet
- Einfache Angebots- und Kalkulationsszenarien
- Standardprodukte mit variablen Komponenten
- Windows-Desktopumgebung (Win32 API)

---

## 2. Systemarchitektur

### 2.1 Hauptkomponenten

```
┌─────────────────────────────────────────────────────────┐
│                    ProductCalculator                    │
│                                                         │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────┐   │
│  │   UI Layer   │    │   Data Layer │    │  Logic   │   │
│  │              │    │              │    │   Layer  │   │
│  │ - Fenster    │    │ - Produkt-   │    │ - Preis- │   │
│  │ - Menü       │    │   Daten      │    │berechnung│   │
│  │ - Dialoge    │    │ - Komponenten│    │ - Ausgabe│   │
│  └──────────────┘    └──────────────┘    └──────────┘   │
│                                                         │
│  ┌──────────────┐    ┌──────────────┐                   │
│  │   Resource   │    │   System     │                   │
│  │   Layer      │    │   Layer      │                   │
│  │ - Icons      │    │ - Win32 API  │                   │
│  │ - Strings    │    │ - GDI        │                   │
│  └──────────────┘    └──────────────┘                   │
└─────────────────────────────────────────────────────────┘
```

### 2.2 Datenfluss

```
Benutzeraktion → UI Layer → Data Layer → Logic Layer → Ausgabe
     ↓              ↓             ↓               ↓
   Menü/Dialog   Produktliste    Preisberechnung  MessageBox
```

---

## 3. Module und Dateien

### 3.1 Hauptdateien

| Datei | Zweck |
|-------|-------|
| `ProductCalculator.cpp` | Einstiegspunkt, Fensterlogik, Nachrichtenschleife |
| `ProductCalculator.h` | Header für globale Variablen und Typen |
| `ProductCalculator.rc` | Ressourcen (Menü, Dialoge, Strings) |
| `Resource.h` | Symbol-IDs für Ressourcen |

### 3.2 Globale Variablen

```cpp
HINSTANCE hInst;                    // Aktuelle Instanz
WCHAR szTitle[MAX_LOADSTRING];      // Titelleistentext
WCHAR szWindowClass[MAX_LOADSTRING]; // Fensterklassenname
```

### 3.3 Hauptfunktionen

| Funktion | Zweck |
|----------|-------|
| `wWinMain()` | Einstiegspunkt, Initialisierung, Nachrichtenschleife |
| `MyRegisterClass()` | Registrierung der Fensterklasse |
| `InitInstance()` | Erstellen und Anzeigen des Hauptfensters |
| `WndProc()` | Fensterprozedur für Nachrichtenverarbeitung |
| `About()` | Info-Dialog (Über uns) |

---

## 4. Datenmodell

### 4.1 Produktstruktur

```
Produkt
├── ProduktID (String/Int)
├── Produktname (String)
├── Basispreis (Double)
└── Komponenten[]
    ├── Komponente1
    │   ├── Name
    │   ├── Preis
    │   └── Optional (Boolean)
    ├── Komponente2
    │   ├── Name
    │   ├── Preis
    │   └── Optional (Boolean)
    └── ...
```

### 4.2 Preisberechnung

```
Verkaufspreis = Basispreis + Σ(Komponentenpreis × Anzahl)
```

---

## 5. Design-Entscheidungen

### 5.1 Warum Win32 API?
- Keine externen Abhängigkeiten (nur Windows SDK)
- Geringer Speicherbedarf
- Einfache Integration in Visual Studio
- Gute Performance für einfache Anwendungen

### 5.2 Warum keine MVC-Architektur?
- Projekt ist klein und einfach
- Trennung von UI und Logik ist nicht notwendig
- Win32 bietet bereits eine klare Trennung durch Fensterprozedur

### 5.3 Ressourcenmanagement
- Alle Strings in Resource-Datei (`ProductCalculator.rc`)
- Icons als separate `.ico`-Dateien
- Menüstruktur über `Resource.h` definiert

---

## 6. Schnittstellen und Erweiterbarkeit

### 6.1 Öffentliche Schnittstellen

| Schnittstelle | Beschreibung |
|---------------|--------------|
| `wWinMain()` | Haupteingangspunkt für die Anwendung |
| `WndProc()` | Nachrichtenverarbeitung (erweiterbar) |

### 6.2 Erweiterungsmöglichkeiten

- **Neue Produkte hinzufügen**: Datenstruktur in Data Layer erweitern
- **Neue Komponenten**: Preisberechnung anpassen
- **Benutzeroberfläche**: Dialoge und Menüs in Resource-Datei
- **Funktionalität**: Neue Fenster oder Dialoge implementieren

### 6.3 Erweiterbare Funktionen

```cpp
// Beispiel: Neue Funktion in WndProc hinzufügen
case WM_COMMAND:
    switch (LOWORD(wParam))
    {
        case IDM_NEW_FEATURE:
            NeueFunktion();
            break;
        // ...
    }
    break;
```

---

## 7. Status und Roadmap

### 7.1 Aktueller Status
- ✅ Grundgerüst der Win32-Anwendung vorhanden
- ✅ Menüstruktur implementiert (Datei → Beenden, Hilfe → Info)
- ⏳ Produktliste zu implementieren
- ⏳ Preisberechnung zu implementieren
- ⏳ UI für Produktauswahl zu implementieren

### 7.2 Nächste Schritte
1. Datenstruktur für Produkte definieren
2. UI für Produktauswahl erstellen
3. Preisberechnungslogik implementieren
4. Testdaten und Beispielprodukte hinzufügen

---

## 8. Konventionen

### 8.1 Namenskonventionen
- **Fensterklasse**: `szWindowClass` (großgeschrieben)
- **Ressourcen-IDs**: `IDM_` für Menüs, `IDI_` für Icons
- **Funktionen**: Groß-/Kleinschreibung nach Win32-Standards

### 8.2 Kommentarstil
```cpp
// FUNKTION: FunktionName()
// ZWECK: Kurze Beschreibung
// KOMMENTARE:
//   - Detailierte Erklärung
```

---

## 9. Referenzen

- [README.md](README.md) – Kurze Übersicht und Installation
- [LICENSE.txt](LICENSE.txt) – MIT License
- Windows SDK Dokumentation für Win32 API

---

*Letzte Änderung: 31.10.2026*
*Autor: Patrick Christe*
