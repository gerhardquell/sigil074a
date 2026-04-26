# CLAUDE.md - Sigil EPUB Editor

## Über dieses Projekt

**Projektname:** Sigil  
**Version:** 0.7.4  
**Beschreibung:** Kostenloser, Open-Source, Multi-Plattform EPUB-Editor  
**Zuständig:** Gerhard  
**Letzte Aktualisierung:** 2026-04-16

---

## Tech Stack

- **Sprache:** C++ (C++11-fähig)
- **Framework:** Qt 6.9 (QtWidgets, QtCore, QtWebEngine)
- **Build-System:** CMake 2.8+
- **XML-Verarbeitung:** Apache Xerces-C++ 3.1+
- **Reguläre Ausdrücke:** PCRE
- **Rechtschreibprüfung:** Hunspell
- **Kompression:** zlib + minizip
- **Web-Engine:** QtWebEngine (Chromium-basiert)

---

## Projektstruktur

```
/data1/u2/sigil074/
├── src/
│   ├── Sigil/              # Hauptanwendung
│   │   ├── main.cpp        # Einstiegspunkt
│   │   ├── MainUI/         # Haupt-UI (MainWindow, BookBrowser, etc.)
│   │   ├── BookManipulation/    # Buch-Logik (Book, FolderKeeper, Metadata)
│   │   ├── ResourceObjects/     # Ressourcen-Klassen (HTML, CSS, Bilder)
│   │   ├── ViewEditors/         # Editoren (BookView, CodeView)
│   │   ├── Tabs/                # Tab-Verwaltung
│   │   ├── Dialogs/             # Dialoge
│   │   ├── Importers/           # EPUB-Import
│   │   ├── Exporters/           # EPUB-Export
│   │   └── Misc/                # Hilfsklassen
│   ├── FlightCrew/         # EPUB-Validierungsbibliothek
│   ├── BoostParts/         # Eingebettete Boost-Bibliotheken
│   ├── Xerces/             # XML-Parser
│   ├── hunspell/           # Rechtschreibprüfung
│   ├── minizip/            # ZIP-Handling
│   ├── pcre/               # Reguläre Ausdrücke
│   ├── tidyLib/            # HTML-Tidy (modifiziert!)
│   ├── zlib/               # Kompression
│   └── utf8-cpp/           # UTF-8 Verarbeitung
├── CMakeLists.txt
└── ChangeLog.txt
```

---

## Wichtige Regeln & Beschränkungen

### Architektur
- **Model-View-Architektur:** Book enthält die Daten, MainWindow die UI
- **Resource-System:** Alle Dateien im EPUB sind Resource-Objekte (HTMLResource, CSSResource, etc.)
- **FolderKeeper:** Verwaltet das Dateisystem eines Buches im Temp-Verzeichnis
- **Tab-basiertes Editing:** ContentTab -> FlowTab (HTML) oder CSS-Tab
- **Zwei Ansichten:** BookView (WYSIWYG) und CodeView (XHTML-Source)

### Code-Qualität
- Qt-Signal/Slot-Muster für Entkopplung
- QSharedPointer für Speicherverwaltung
- Thread-sicherer Zugriff auf Ressourcen (QMutex)

### Wichtige Hinweise
- **TidyLib ist modifiziert!** Nicht mit Upstream-Tidy austauschbar
- Alle Boost-Bibliotheken sind als Source eingebettet (src/BoostParts)
- Xerces wird für strikte XML-Verarbeitung verwendet

---

## Nützliche Befehle

```bash
# Build-Verzeichnis erstellen
mkdir build && cd build

# CMake konfigurieren
cmake ..

# Kompilieren
make -j4

# Mit Debug-Infos
cmake -DCMAKE_BUILD_TYPE=Debug ..
```

---

## Kernkonzepte

### 1. Book-Klasse
Repräsentiert ein geladenes EPUB. Enthält:
- FolderKeeper (Dateiverwaltung)
- OPF- und NCX-Ressourcen
- Metadaten
- Modifikations-Status

### 2. Resource-System
Hierarchie:
- `Resource` (Basis)
  - `HTMLResource`
  - `CSSResource`
  - `ImageResource`
  - `OPFResource`
  - `NCXResource`
  - etc.

### 3. View States
- `ViewState_BookView` (10) - WYSIWYG-Ansicht
- `ViewState_CodeView` (30) - Code-Editor

### 4. EPUB-Struktur
- Temporäres Arbeitsverzeichnis
- OEBPS-Ordner mit Text/, Styles/, Images/
- META-INF/container.xml

---

## Notizen

- Version 0.7.4 aus 2013
- Qt 6.9 (migriert von Qt 5.2)
- FlightCrew für EPUB-Validierung integriert
- Plattform-spezifische Workarounds für Windows (Clipboard) und macOS (Menüs)
