# sigil-md-workflow

Markdown-zu-EPUB Workflow mit ReText, pandoc und Makefile.

## Voraussetzungen

- **pandoc** >= 3.0 (`pandoc --version`)
- **ReText** >= 8.0 (optional, zum Editieren)
- **make**

## Verzeichnisstruktur

```
sigil-md-workflow/
├── README.md
├── Makefile
├── metadata.yaml          # Buch-Metadaten (Titel, Autor, Sprache)
├── style.css              # EPUB-Stylesheet
└── chapters/              # Markdown-Kapitel, numerisch sortiert
    ├── 01-einleitung.md
    ├── 02-hauptteil.md
    └── 03-schluss.md
```

## Verwendung

```bash
# EPUB erzeugen
make

# EPUB löschen
make clean

# Mit ReText bearbeiten
retext chapters/01-einleitung.md
```

## metadata.yaml

```yaml
title: "Mein Buch"
author: "Gerhard"
lang: de
date: 2026-04-22
```

## style.css

CSS wird 1:1 ins EPUB übernommen. Standard-Styles für gängige HTML-Elemente.

## Kapitel

Kapitel in `chapters/` werden numerisch sortiert (`01-`, `02-`, ...).
Jede `.md`-Datei wird ein EPUB-Kapitel.
Überschrift der ersten Ebene (`#`) bestimmt den Kapiteltitel im Inhaltsverzeichnis.

## Sigil als Nachbearbeitung

Falls pandoc-Ausgabe manuelles Nachbessern braucht:
1. EPUB in Sigil öffnen
2. XHTML/CSS anpassen
3. Neu speichern

## Referenz

- pandoc EPUB-Optionen: https://pandoc.org/MANUAL.html#producing-epub
- ReText: https://github.com/retext-project/retext
