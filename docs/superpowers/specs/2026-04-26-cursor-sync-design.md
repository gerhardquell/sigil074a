# Cursor-Sync: CodeView → Preview

**Datum:** 2026-04-26
**Status:** Approved
**Autor:** Gerhard + Claude

## Ziel

Live-Synchronisation der Cursorposition im CodeView-Editor mit dem Preview-Widget. Wenn der Cursor im CodeView bewegt wird, scrollt das Preview zum entsprechenden HTML-Element.

## Entscheidung

- **Richtung:** Nur CodeView → Preview (unidirektional)
- **Trigger:** Jede Cursorbewegung (mit 100ms Debounce)
- **Mapping:** Element-basiert via `GetCaretLocation()` + CSS-Selektor
- **Scroll:** `scrollIntoView({block: 'center'})` im Preview

## Architektur

```
CodeViewEditor                    FlowTab                    PreviewWidget
─────────────                     ───────                    ─────────────
cursorPositionChanged() ──→ onCursorPositionChanged() ──→ scrollToCaretElement()
        │                              │
  GetCaretLocation()           ElementIndex → CSS-Selektor
  (bestehend)                  z.B. "p:nth-of-type(3)"
```

## Flow

1. CodeView feuert `cursorPositionChanged` (Qt-Signal, bereits an `FlowTab::onCursorPositionChanged` verbunden)
2. `onCursorPositionChanged()` startet `m_cursorSyncTimer` (100ms single-shot)
3. Timer feuert → `FlowTab::syncPreviewToCursor()`
4. `syncPreviewToCursor()` ruft `m_CodeViewEditor->GetCaretLocation()` auf
5. `ElementIndexToCssSelector()` konvertiert ElementIndex-Liste zu CSS-Selektor
6. `m_PreviewWidget->scrollToCaretElement(selector)` führt JS im Preview aus
7. JS: `document.querySelector(selector).scrollIntoView({block: 'center'})`

## Neue Methoden

### FlowTab

| Methode | Beschreibung |
|---------|-------------|
| `syncPreviewToCursor()` | Holt Caret-Location, konvertiert, ruft Preview |
| `ElementIndexToCssSelector(QList<ElementIndex>)` | ElementIndex-Liste → CSS-Selektor-String |

### FlowTab — Neue Member

| Member | Beschreibung |
|--------|-------------|
| `QTimer *m_cursorSyncTimer` | 100ms debounce timer |

### PreviewWidget

| Methode | Beschreibung |
|---------|-------------|
| `scrollToCaretElement(const QString &selector)` | JS: querySelector + scrollIntoView |

## ElementIndex → CSS-Selektor Konvertierung

`GetCaretLocation()` liefert: `[{name:"html", index:0}, {name:"body", index:0}, {name:"p", index:2}, {name:"em", index:0}]`

Regeln:
1. `html` und `body` überspringen (zu hoch)
2. Nur Block-Level-Elemente als Anker (`p`, `div`, `h1`-`h6`, `blockquote`, `ul`, `ol`, `li`, `table`, `section`, `article`, `header`, `footer`, `aside`, `nav`, `figure`, `figcaption`, `dl`, `pre`, `hr`, `address`)
3. CSS `:nth-of-type(N)` — `index` ist 0-basiert, `:nth-of-type` ist 1-basiert, also N = index + 1
4. Ergebnis: `"p:nth-of-type(3)"`
5. Wenn nur Inline-Elemente gefunden: Elter-Block-Element + Kind-Kombinator (z.B. `"p:nth-of-type(3) > em:nth-of-type(1)"`)

## Edge Cases

| Fall | Behandlung |
|------|-----------|
| Cursor außerhalb `<body>` | Kein Scroll, still ignorieren |
| Preview noch am Laden | Selector speichern, nach `loadFinished` nachholen |
| Element nicht gefunden | JS `if (element)` check, still tolerieren |
| Preview nicht sichtbar | `m_PreviewWidget->isVisible()` check, skip |
| Debounce vs. Preview-Update | Sync nur wenn `!m_isLoading` |

## Was NICHT gemacht wird

- Kein Reverse-Sync (Preview → CodeView)
- Keine Scroll-Synchronisation (nur Position → Element-Scroll)
- Keine ID-Injection ins HTML
- Keine Markierung/Hervorhebung im Preview

## Dateien

| Datei | Änderung |
|-------|---------|
| `src/Sigil/Tabs/FlowTab.h` | Timer + Methoden deklarieren |
| `src/Sigil/Tabs/FlowTab.cpp` | Implementierung |
| `src/Sigil/ViewEditors/PreviewWidget.h` | `scrollToCaretElement()` deklarieren |
| `src/Sigil/ViewEditors/PreviewWidget.cpp` | JS-Ausführung implementieren |
