# User Templates Design: ~/.sigil/ Configuration

**Datum:** 2026-04-17
**Autor:** Gerhard (mit Claude Code)
**Status:** Approved

---

## Ziel

Benutzerdefinierte Templates in `~/.sigil/`, die beim Erstellen neuer EPUBs automatisch verwendet werden. Drei Dateien:

| Datei | Zweck | Fallback |
|-------|-------|----------|
| `default.xhtml` | HTML-Template für neue Sections | Hardcoded `EMPTY_HTML_FILE` |
| `style.css` | Default-Stylesheet | Leerstring |
| `impressum.xhtml` | Impression/Impressum-Seite | Kein Impressum |

---

## Architektur

### Neue Klasse: UserTemplates (Singleton)

**Dateien:** `src/Sigil/Misc/UserTemplates.h`, `src/Sigil/Misc/UserTemplates.cpp`

```cpp
class UserTemplates
{
public:
    static UserTemplates& instance();

    QString defaultHtml() const;
    QString defaultCss() const;
    QString impressumHtml() const;
    bool hasImpressum() const;

    QString adjustCssLinks(const QString &html, const QString &cssFilename) const;

    static QString configDir();  // ~/.sigil/

private:
    UserTemplates();
    QString readFile(const QString &filename) const;

    QString m_defaultHtml;
    QString m_defaultCss;
    QString m_impressumHtml;
    bool m_hasImpressum;
};
```

### Verantwortlichkeiten

- **Konstruktor:** Liest alle drei Dateien aus `~/.sigil/` einmalig beim Start
- **Fehlende Dateien:** Fallback ohne Fehler/Warnung
- **Fehlendes Verzeichnis:** Fallback, kein automatisches Anlegen von `~/.sigil/`
- **`adjustCssLinks()`:** Ersetzt CSS-Dateinamen in `href="../Styles/Style.css"` durch den EPUB-internen Namen (z.B. `Style0001.css`)

---

## Änderungen an Book

**Datei:** `src/Sigil/BookManipulation/Book.cpp`

### CreateEmptyHTMLFile()

```cpp
// Vorher: hardcoded EMPTY_HTML_FILE
// Nachher:
QString html = UserTemplates::instance().defaultHtml();
html = UserTemplates::instance().adjustCssLinks(html, cssFilename);
```

### CreateEmptyCSSFile()

```cpp
// Vorher: WriteUnicodeTextFile("", fullfilepath)
// Nachher:
QString css = UserTemplates::instance().defaultCss();
Utility::WriteUnicodeTextFile(css, fullfilepath);
```

### Neues CreateImpressumFile()

- Wird aufgerufen wenn `UserTemplates::instance().hasImpressum()` true
- Erstellt `Impressum.xhtml` in OEBPS/Text/
- CSS-Link wird via `adjustCssLinks()` angepasst
- Als erste HTML-Datei im EPUB (vor Section0001.xhtml)
- Keine Guide-Semantik (Impressum ist kein Standard-EPUB-Guide-Typ)

### Aufruf-Reihenfolge

In Book-Konstruktor oder FolderKeeper::CreateInfrastructureFiles():
1. Basis-Struktur erstellen (META-INF, OEBPS)
2. OPF + NCX erstellen
3. Stylesheet erstellen (mit User-CSS)
4. Impressum erstellen (wenn hasImpressum())
5. Erste Section erstellen (mit User-HTML)

---

## Dateiname und Position des Impressums

- **Dateiname:** `Impressum.xhtml` (fest, wie `cover.xhtml`)
- **Position:** Erste HTML-Datei in OEBPS/Text/
- **Spine:** Automatisch in Reading-Order aufgenommen
- **Guide:** Keine Semantik zugeordnet

---

## Config-Verzeichnis

- **Pfad:** `QDir::homePath() + "/.sigil"`
- **Wird nicht automatisch erstellt** — User muss es manuell anlegen
- **Keine UI** für die Verwaltung in dieser Version

---

## Nicht-Ziele (YAGNI)

- Kein UI für Template-Verwaltung
- Kein automatisches Anlegen von `~/.sigil/`
- Kein Hot-Reloading bei Dateiänderungen zur Laufzeit
- Keine Validierung der User-Dateien (XHTML/CSS)
