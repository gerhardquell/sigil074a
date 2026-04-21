# Block Translator Design Spec

**Datum:** 2026-04-21
**Feature:** XHTML-Block-Übersetzung via Rechtsklick-Context-Menü
**Autor:** Gerhard + Claude

---

## Zusammenfassung

Rechtsklick auf XHTML-Block im CodeView → Context-Menü "Übersetzen" → EN→DE / DE→EN mit Modell-Auswahl. Übersetzung ersetzt Text In-Place via sigoREST API.

---

## Architektur

### Neue Klasse: `Translator` (QObject)

Pfad: `src/Sigil/Misc/Translator.h`, `src/Sigil/Misc/Translator.cpp`

Kapselt sigoREST-Client, Block-Erkennung, Platzhalter-Verarbeitung, Prompt-Logik.

```
class Translator : public QObject
  Q_OBJECT

Signale:
  translationReady(QString newBlock)
  translationError(QString message)

Öffentliche Methoden:
  Translator(QObject *parent)
  void translate(QString blockText, QString direction, QString model)
  QStringList availableModels()           // gecachte Modell-Liste
  void refreshModels()                    // /v1/models abrufen
  bool isServerAvailable()                // Ping-Check

Private Methoden:
  QString encodePlaceholders(QString html)    // <em>text</em> → ⟦EM⟦text⟧EM⟧
  QString decodePlaceholders(QString text)    // ⟦EM⟦text⟧EM⟧ → <em>text</em>
  QString escapeHtml(QString text)            // < → &lt;, & → &amp;
  QString buildPrompt(QString text, QString direction)

Private Member:
  sigorest::QtClient *m_client
  QStringList m_cachedModels
  QString m_serverUrl
```

### Abhängigkeit: sigoREST Qt Client

Bibliothek in `/u/go-projekte/sigoREST/clients/cpp/`:
- `core/include/sigorest/models.hpp` — ChatMessage, ChatResponse, ChatChoice, ChatUsage
- `qt/include/sigorest/qt/client.hpp` — QtClient (QObject, Signale: completed, error)

Einbindung: Source-Dateien als Subdirectory ins Sigil-Projekt oder als installierte Lib. CMakeLists.txt muss `sigorest::QtClient` bekannt machen.

---

## Block-Erkennung

### Unterstützte Block-Tags

`p`, `h1`-`h6`, `li`, `div`, `blockquote`, `td`, `th`, `span`, `dt`, `dd`, `pre`, `figcaption`

### Algorithmus

1. Cursor-Position im Plain Text (QTextCursor)
2. Rückwärts suchen nach öffnendem Tag `<tag` aus der Liste oben
3. Vorwärts suchen nach geschlossenem Tag `</tag>`
4. Ergebnis: `{startPos, endPos, tagName, innerContent}`

### Edge Cases

- Cursor nicht in einem Block-Tag → Menü-Eintrag deaktiviert
- Verschachtelte Blöcke (z.B. `<li>` in `<ul>`) → innerstes Block-Tag gewinnen
- Self-closing Tags (`<br/>`, `<img/>`) → überspringen, kein Block

---

## Inline-Tag Platzhalter-Verfahren

### Problem

XHTML-Blöcke enthalten oft inline-Tags: `<p>Hello <em>wonderful</em> world</p>`. Reine Text-Extraktion verliert die Tags.

### Lösung

Inline-Tags als Platzhalter kodieren, LLM übersetzt drumherum, Platzhalter zurückdekodieren.

### Platzhalter-Format

```
<em>wonderful</em>    →    ⟦EM⟦wonderful⟧EM⟧
<strong>bold</strong>  →    ⟦STRONG⟦bold⟧STRONG⟧
<a href="x">link</a>  →    ⟦A⟦link⟧A⟧
```

### Inline-Tags die als Platzhalter behandelt werden

`em`, `strong`, `b`, `i`, `u`, `a`, `span`, `sub`, `sup`, `code`, `small`, `mark`, `abbr`, `cite`, `q`

### Algorithmus

1. Block-Inneres nehmen (zwischen `>` des öffnenden und `<` des schließenden Tags)
2. Regex: Alle öffnenden/schließenden inline-Tags finden
3. Öffnendes Tag `<em>` → `⟦EM⟦`, schließendes `</em>` → `⟧EM⟧`
4. Attribute beim öffnenden Tag merken für Reintegration (z.B. `<a href="...">`)
5. LLM bekommt Text mit Platzhaltern
6. Antwort: Platzhalter zurückdekodieren → HTML mit Original-Tags

### Reintegration

Nach Übersetzung:
1. `⟦EM⟦wunderbare⟧EM⟧` → `<em>wunderbare</em>`
2. `⟦A⟦Link⟧A⟧` → `<a href="original-url">Link</a>` (gespeicherte Attribute wieder einsetzen)
3. Übersetzten Text HTML-escapen (Text-Teile, nicht Tags)
4. Neuen Block: `<tag attr="...">übersetzter Inhalt</tag>`

---

## Context-Menü

### Platzierung

Zwischen Standard-QPlainTextEdit-Einträgen (Copy/Paste/etc.) und Custom-Einträgen (Reformat, Spell Check), mit Separator davor.

### Struktur

```
─────────────────────────     ← Separator
Übersetzen →
  English → Deutsch →
    claude-h
    gpt41
    ollama-llama3.3
    ...
  Deutsch → English →
    claude-h
    gpt41
    ollama-llama3.3
    ...
```

### Implementierung

`AddTranslateContextMenu(QMenu *menu)` in CodeViewEditor:

1. Separator einfügen
2. `QMenu *translateMenu = menu->addMenu(tr("Übersetzen"))`
3. `QMenu *enDeMenu = translateMenu->addMenu(tr("English → Deutsch"))`
4. `QMenu *deEnMenu = translateMenu->addMenu(tr("Deutsch → English"))`
5. Für jedes Modell aus `Translator::availableModels()`:
   - QAction in enDeMenu und deEnMenu
   - QSignalMapper für Richtung + Modell
6. Wenn `availableModels().isEmpty()` → gesamtes Submenü deaktiviert

### QSignalMapper (Qt 6: mappedString)

```
m_translateMapper = new QSignalMapper(this);
connect(m_translateMapper, SIGNAL(mappedString(const QString &)),
        this, SLOT(TranslateBlock(const QString &)));

// Für jeden Eintrag:
action = new QAction(modelName, menu);
connect(action, SIGNAL(triggered()), m_translateMapper, SLOT(map()));
m_translateMapper->setMapping(action, direction + "|" + modelName);
```

---

## Prompt-Design

### System-Prompt

```
You are a translation engine. Translate the following text to {target_language}.
Output only the translated text, no explanations.
Preserve the ⟦TAG⟦...⟧TAG⟧ placeholders exactly as they appear — do not translate, remove, or modify them.
```

- `{target_language}` = "German" oder "English"

### User-Prompt

```
{text with ⟦PLACEHOLDER⟧ markers}
```

### Parameter

- `max_tokens`: 2000 (ausreichend für einen Block)
- `temperature`: 0.3 (konsistente Übersetzung)

---

## Settings

### Neuer Eintrag in SettingsStore

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `sigorest_server_url` | QString | `http://localhost:9080` | sigoREST Server-URL |

### SettingsStore-Erweiterung

```cpp
QString SettingsStore::sigorestServerUrl();
void SettingsStore::setSigorestServerUrl(const QString &url);
```

### Preferences-UI

Neues Widget: `TranslationWidget` (erbt `PreferencesWidget`)

Pfad: `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.h/.cpp/.ui`

Felder:
- Server-URL (QLineEdit) mit Default-Button
- Status-Anzeige: "Server erreichbar" / "Nicht erreichbar" (QPushButton zum Testen)

Registrierung in `Preferences.cpp`:

```cpp
appendPreferenceWidget(new TranslationWidget());
```

---

## Datenfluss

```
1. Rechtsklick im CodeView
2. contextMenuEvent() → AddTranslateContextMenu()
3. Translator::availableModels() → gecachte Modell-Liste
4. User klickt "EN→DE / claude-h"
5. CodeViewEditor::TranslateBlock("en→de|claude-h")
6. Block-Erkennung in CodeViewEditor: Cursor → startPos, endPos, innerContent
7. CodeViewEditor ruft Translator::translate(innerContent, "en→de", "claude-h")
8.   encodePlaceholders(innerContent)
9.   buildPrompt(encoded, "en→de")
10.  sigorest::QtClient::chatCompletion("claude-h", messages, 2000, systemPrompt)
11.  QtClient::completed → Translator::onTranslationReady
12.  decodePlaceholders(translatedText)
13.  escapeHtml(textParts)
14.  emit translationReady(newBlock)
15. CodeViewEditor empfängt Signal, ersetzt Block (startPos..endPos) via QTextCursor
```

---

## Fehlerbehandlung

| Situation | Verhalten |
|-----------|-----------|
| sigoREST nicht erreichbar | Menü deaktiviert. Status in TranslationWidget zeigen. |
| API-Fehler (Circuit Breaker, Rate Limit) | `translationError` Signal → QMessageBox |
| Block nicht erkannt | Menü-Eintrag deaktiviert |
| Übersetzung enthält keine Platzhalter mehr | Warnung im Log, Original behalten |
| Übersetzung leer | Fehlermeldung, Original behalten |
| Request läuft bereits | QtClient gibt "Request already in progress" → User-Feedback |

---

## Modell-Caching

- Beim Sigil-Start: `Translator::refreshModels()` → GET `/v1/models`
- Gecacht in `m_cachedModels` (QStringList)
- Bei Server-Fehler: leere Liste → Menü deaktiviert
- Manueller Refresh: Button im TranslationWidget Settings

---

## Dateien

### Neu

| Datei | Zweck |
|-------|-------|
| `src/Sigil/Misc/Translator.h` | Translator-Klasse Header |
| `src/Sigil/Misc/Translator.cpp` | Translator-Klasse Implementierung |
| `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.h` | Settings-Widget Header |
| `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.cpp` | Settings-Widget Implementierung |
| `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.ui` | Qt Designer Form |

### Geändert

| Datei | Änderung |
|-------|----------|
| `src/Sigil/ViewEditors/CodeViewEditor.h` | Translator-Member, AddTranslateContextMenu(), TranslateBlock-Slot, QSignalMapper |
| `src/Sigil/ViewEditors/CodeViewEditor.cpp` | Context-Menü erweitern, Slot-Implementierung |
| `src/Sigil/Misc/SettingsStore.h` | sigorestServerUrl() / setSigorestServerUrl() |
| `src/Sigil/Misc/SettingsStore.cpp` | Implementierung der neuen Settings-Methoden |
| `src/Sigil/Dialogs/Preferences.cpp` | TranslationWidget registrieren |
| `CMakeLists.txt` | Neue Source-Dateien, sigoREST Client-Pfade |

### Extern

| Pfad | Zweck |
|------|-------|
| `/u/go-projekte/sigoREST/clients/cpp/core/` | sigoREST Core Client (models.hpp, client.hpp, client.cpp) |
| `/u/go-projekte/sigoREST/clients/cpp/qt/` | sigoREST Qt Client (client.hpp, client.cpp) |

---

## CMake-Einbindung

sigoREST Client-Quellen per `add_subdirectory` einbinden. Pfade relativ zum Projekt-Root oder als absolute Pfade konfigurierbar.

```cmake
# In CMakeLists.txt
set(SIGOREST_CLIENT_DIR "/u/go-projekte/sigoREST/clients/cpp")
add_subdirectory(${SIGOREST_CLIENT_DIR} ${CMAKE_BINARY_DIR}/sigorest-client)
target_link_libraries(sigil PRIVATE sigorest-qt)
```

Die sigoREST CMakeLists.txt muss ein `sigorest-qt` Target exportieren (Core + Qt Client).
