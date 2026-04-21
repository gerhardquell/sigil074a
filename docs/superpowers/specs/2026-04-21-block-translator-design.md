# Block Translator Design Spec

**Datum:** 2026-04-21
**Feature:** XHTML-Block-Übersetzung via Rechtsklick-Context-Menü
**Autor:** Gerhard + Claude

---

## Zusammenfassung

Rechtsklick auf XHTML-Block im CodeView → Context-Menü "Translate" → EN→DE / DE→EN mit Modell-Auswahl. Übersetzung ersetzt Text In-Place via OpenAI-kompatibler API.

---

## Architektur

### Klasse: `Translator` (QObject)

Pfad: `src/Sigil/Misc/Translator.h`, `src/Sigil/Misc/Translator.cpp`

Kapselt HTTP-Client (QNetworkAccessManager), Platzhalter-Verarbeitung, Prompt-Logik.

```
class Translator : public QObject
  Q_OBJECT

Signale:
  translationReady(QString newBlock)
  translationError(QString message)
  modelsRefreshed()

Öffentliche Methoden:
  Translator(QObject *parent)
  void translate(QString blockText, QString direction, QString model)
  QStringList availableModels()           // gecachte Modell-Liste
  void refreshModels()                    // /v1/models abrufen
  bool isServerAvailable()

Private Methoden:
  QString encodePlaceholders(QString html)
  QString decodePlaceholders(QString text)
  QString escapeHtml(QString text)
  QString buildSystemPrompt(QString direction)
  QString buildUserPrompt(QString text)
  QNetworkRequest createRequest(QString endpoint)
  QStringList loadModelWhitelist() const
  void applyWhitelistFilter()

Private Member:
  QNetworkAccessManager *m_networkManager
  QNetworkReply *m_modelsReply
  QNetworkReply *m_translateReply
  QStringList m_cachedModels
  QString m_serverUrl
  QMap<QString, QString> m_savedTagAttributes

Statische Konstanten:
  BLOCK_TAGS  — p, div, h1-h6, ul, ol, li, blockquote, pre, table, thead, tbody, tr, td, th
  INLINE_TAGS — em, strong, b, i, u, a, span, sub, sup, code, small, mark, abbr, cite, q
```

### HTTP-Client

QNetworkAccessManager statt sigoREST QtClient. Direkte HTTP-Aufrufe an OpenAI-kompatible API:
- `GET /v1/models` — Modell-Liste
- `POST /v1/chat/completions` — Übersetzung

---

## Block-Erkennung

### Algorithmus (in CodeViewEditor)

1. Cursor-Position im Plain Text (QTextCursor)
2. Rückwärts suchen nach öffnendem Block-Tag
3. Vorwärts suchen nach geschlossenem Block-Tag
4. Ergebnis: startPos, endPos des kompletten Blocks inkl. Tags

### Unterstützte Block-Tags

`p`, `div`, `h1`-`h6`, `ul`, `ol`, `li`, `blockquote`, `pre`, `table`, `thead`, `tbody`, `tr`, `td`, `th`

---

## Inline-Tag Platzhalter-Verfahren

### Platzhalter-Format (ASCII)

```
<em>wonderful</em>       →    <<EM:0>>wonderful<</EM:0>>
<strong>bold</strong>     →    <<STRONG:0>>bold<</STRONG:0>>
<a href="x">link</a>     →    <<A:0>>link<</A:0>>         (href in m_savedTagAttributes)
```

Nummerierung pro Tag-Typ für eindeutige Zuordnung. Attribute werden in `m_savedTagAttributes` gespeichert (Key: `"0_A"`, Value: `href="x"`).

### Inline-Tags

`em`, `strong`, `b`, `i`, `u`, `a`, `span`, `sub`, `sup`, `code`, `small`, `mark`, `abbr`, `cite`, `q`

### Block-Tags

Block-Tags bleiben im Text erhalten. Der LLM bekommt sie und muss sie bewahren (System-Prompt-Anweisung). Grund: Entfernen führte zu Datenverlust — Tags wurden nicht wiederhergestellt.

### Algorithmus

1. Inline-Tags als Platzhalter kodieren (mit Attribut-Speicherung)
2. Block-Tags im Text belassen
3. LLM bekommt Text mit Platzhaltern + Block-Tags
4. Antwort: Platzhalter zurückdekodieren, Block-Tags durchlaufen unangetastet

### Reintegration

Nach Übersetzung:
1. `<<EM:0>>wunderbare<</EM:0>>` → `<em>wunderbare</em>`
2. `<<A:0>>Link<</A:0>>` → `<a href="original-url">Link</a>` (gespeicherte Attribute)
3. Übersetzten Text HTML-escapen (nur Text-Teile innerhalb der Tags)
4. Space zwischen Tag-Name und Attribut wird explizit gesetzt: `<%1 %2>` (verhindert `<ahref="">`)

---

## Model Whitelist

### Konfiguration

`~/.sigil/translator_models.json` — optional.

```json
{
  "models": [
    "claude-sonnet-4-6",
    "gemini-2.5-flash",
    "gpt-4.1-mini"
  ]
}
```

### Logik

- Datei existiert → nur gelistete Modelle im Menü (Schnittmenge mit Server-Liste)
- Datei fehlt → alle Server-Modelle

### Ablauf

1. `refreshModels()` holt Modell-Liste vom Server
2. `onModelsReplyFinished()` parst JSON, füllt `m_cachedModels`
3. `applyWhitelistFilter()` lädt Whitelist, filtert `m_cachedModels`
4. `modelsRefreshed()` Signal → UI aktualisiert

---

## Context-Menü

### Struktur

```
─────────────────────────     ← Separator
Translate →
  English → Deutsch →
    claude-sonnet-4-6
    gemini-2.5-flash
    ...
  Deutsch → English →
    claude-sonnet-4-6
    gemini-2.5-flash
    ...
```

### QSignalMapper (Qt 6: mappedString)

Format: `"direction|model"` → z.B. `"en→de|claude-sonnet-4-6"`

---

## Prompt-Design

### System-Prompt

```
You are a translation engine. Translate the following text to {target_language}.
Output only the translated text, no explanations.
Preserve all HTML tags (like <p>, <div>, <h1>) exactly as they appear —
do not translate, remove, or modify them or their attributes.
Preserve all markers like <<EM:0>> and <</EM:0>> exactly as they appear —
do not translate, remove, or modify them.
```

### Parameter

- `max_tokens`: 2000
- `temperature`: 0.3

---

## Settings

### SettingsStore

| Key | Typ | Default | Beschreibung |
|-----|-----|---------|--------------|
| `Translation/sigorest_server_url` | QString | `http://localhost:9080` | API Server-URL |

### Preferences-UI

`TranslationWidget` (erbt `PreferencesWidget`) — Server-URL, Test-Button, Default-Button, Status-Anzeige.

---

## Datenfluss

```
1. Rechtsklick im CodeView
2. contextMenuEvent() → AddTranslateContextMenu()
3. Translator::availableModels() → gefilterte Modell-Liste
4. User klickt "EN→DE / claude-sonnet-4-6"
5. CodeViewEditor::TranslateBlock("en→de|claude-sonnet-4-6")
6. FindCurrentBlock() → startPos, endPos
7. Translator::translate(blockText, "en→de", "claude-sonnet-4-6")
8.   encodePlaceholders(blockText)
9.   POST /v1/chat/completions
10.  onTranslateReplyFinished() → decodePlaceholders()
11.  emit translationReady(newBlock)
12. CodeViewEditor::OnTranslationReady() → QTextCursor ersetzt Block
```

---

## Dateien

### Neu

| Datei | Zweck |
|-------|-------|
| `src/Sigil/Misc/Translator.h` | Translator-Klasse Header |
| `src/Sigil/Misc/Translator.cpp` | Translator-Klasse Implementierung |
| `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.h` | Settings-Widget Header |
| `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.cpp` | Settings-Widget Implementierung |
| `src/Sigil/Form_Files/PTranslationWidget.ui` | Qt Designer Form |

### Geändert

| Datei | Änderung |
|-------|----------|
| `src/Sigil/ViewEditors/CodeViewEditor.h` | Translator-Member, Slots, QSignalMapper |
| `src/Sigil/ViewEditors/CodeViewEditor.cpp` | Context-Menü, Block-Erkennung, Slot-Implementierung |
| `src/Sigil/Misc/SettingsStore.h` | sigorestServerUrl() / setSigorestServerUrl() |
| `src/Sigil/Misc/SettingsStore.cpp` | Implementierung |
| `src/Sigil/Dialogs/Preferences.cpp` | TranslationWidget registrieren |
| `src/Sigil/CMakeLists.txt` | Neue Source-Dateien |
