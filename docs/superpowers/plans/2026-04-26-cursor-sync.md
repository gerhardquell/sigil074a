# Cursor-Sync Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sync CodeView cursor position to Preview — cursor moves in code, Preview scrolls to corresponding HTML element.

**Architecture:** FlowTab connects `cursorPositionChanged` signal → 100ms debounce timer → `syncPreviewToCursor()` → converts `GetCaretLocation()` ElementIndex list to CSS selector → `PreviewWidget::scrollToCaretElement()` runs JS `querySelector` + `scrollIntoView`.

**Tech Stack:** C++11, Qt 5 (QTimer, QWebEngineView, runJavaScript), existing ViewEditor::ElementIndex system.

---

## File Structure

| File | Action | Responsibility |
|------|--------|---------------|
| `src/Sigil/ViewEditors/PreviewWidget.h` | Modify | Add `scrollToCaretElement()` declaration |
| `src/Sigil/ViewEditors/PreviewWidget.cpp` | Modify | Implement `scrollToCaretElement()` with JS |
| `src/Sigil/Tabs/FlowTab.h` | Modify | Add timer, `syncPreviewToCursor()`, `ElementIndexToCssSelector()` |
| `src/Sigil/Tabs/FlowTab.cpp` | Modify | Implement cursor sync logic + wire signals |

---

### Task 1: Add `scrollToCaretElement()` to PreviewWidget

**Files:**
- Modify: `src/Sigil/ViewEditors/PreviewWidget.h:50` (after `scrollToElement` declaration)
- Modify: `src/Sigil/ViewEditors/PreviewWidget.cpp:129` (after `scrollToElement` method)

- [ ] **Step 1: Add declaration to PreviewWidget.h**

After line 50 (`void scrollToElement(const QString &elementId);`), add:

```cpp
    void scrollToCaretElement(const QString &cssSelector);
```

- [ ] **Step 2: Implement in PreviewWidget.cpp**

After the `scrollToElement` method (after line 143), add:

```cpp
void PreviewWidget::scrollToCaretElement(const QString &cssSelector)
{
    if (cssSelector.isEmpty() || m_isLoading) {
        return;
    }

    QString js = QString(
        "var element = document.querySelector('%1');"
        "if (element) {"
        "    element.scrollIntoView({behavior: 'instant', block: 'center'});"
        "}"
    ).arg(cssSelector);

    m_webView->page()->runJavaScript(js);
}
```

Note: Uses `behavior: 'instant'` (not `'smooth'`) for cursor-following — smooth animation fights fast cursor movement.

- [ ] **Step 3: Build and verify compilation**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build, no errors related to PreviewWidget.

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/ViewEditors/PreviewWidget.h src/Sigil/ViewEditors/PreviewWidget.cpp
git commit -m "feat(preview): add scrollToCaretElement() for CSS selector-based scrolling"
```

---

### Task 2: Add `ElementIndexToCssSelector()` to FlowTab

**Files:**
- Modify: `src/Sigil/Tabs/FlowTab.h` (add method declaration)
- Modify: `src/Sigil/Tabs/FlowTab.cpp` (add implementation)

- [ ] **Step 1: Add method declaration to FlowTab.h**

In the `private:` section (after line 355 `void ConnectSignalsToSlots();`), add:

```cpp
    QString ElementIndexToCssSelector(const QList< ViewEditor::ElementIndex > &hierarchy) const;
```

Also add `#include <QSet>` near the top includes if not already present. Check first — if missing, add after line 29.

- [ ] **Step 2: Implement `ElementIndexToCssSelector()` in FlowTab.cpp**

Add before `ConnectSignalsToSlots()` (before line 1056). This is a static-style helper, place it with the other private helpers:

```cpp
QString FlowTab::ElementIndexToCssSelector(const QList< ViewEditor::ElementIndex > &hierarchy) const
{
    // Block-level HTML elements that CSS selectors should anchor on
    static const QSet<QString> blockElements = QSet<QString>()
        << "p" << "div" << "h1" << "h2" << "h3" << "h4" << "h5" << "h6"
        << "blockquote" << "ul" << "ol" << "li" << "table" << "section"
        << "article" << "header" << "footer" << "aside" << "nav"
        << "figure" << "figcaption" << "dl" << "pre" << "hr" << "address"
        << "main" << "details" << "summary";

    // Skip html, body — too high-level for useful scrolling
    // Walk from root to leaf, find the deepest block-level element
    int blockIndex = -1;
    for (int i = hierarchy.size() - 1; i >= 0; --i) {
        if (blockElements.contains(hierarchy[i].name)) {
            blockIndex = i;
            break;
        }
    }

    if (blockIndex < 0) {
        // No block element found (cursor might be in head, before body, etc.)
        return QString();
    }

    // Build selector for the block element
    const ViewEditor::ElementIndex &block = hierarchy[blockIndex];
    // ElementIndex.index is 0-based, :nth-of-type is 1-based
    QString selector = QString("%1:nth-of-type(%2)")
                           .arg(block.name)
                           .arg(block.index + 1);

    // If there's an inline child after the block, append it for precision
    if (blockIndex + 1 < hierarchy.size()) {
        const ViewEditor::ElementIndex &child = hierarchy[blockIndex + 1];
        selector += QString(" > %1:nth-of-type(%2)")
                        .arg(child.name)
                        .arg(child.index + 1);
    }

    return selector;
}
```

- [ ] **Step 3: Build and verify compilation**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build.

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/Tabs/FlowTab.h src/Sigil/Tabs/FlowTab.cpp
git commit -m "feat(flowtab): add ElementIndexToCssSelector() for cursor sync mapping"
```

---

### Task 3: Add debounce timer and `syncPreviewToCursor()` to FlowTab

**Files:**
- Modify: `src/Sigil/Tabs/FlowTab.h` (add timer member + slot)
- Modify: `src/Sigil/Tabs/FlowTab.cpp` (implement + wire up)

- [ ] **Step 1: Add timer member and slot to FlowTab.h**

In the `private slots:` section (after line 327, `void onCursorPositionChanged();`), add:

```cpp
    void syncPreviewToCursor();
```

In the `private:` member variables section (after line 404, `bool m_initialLoad;`), add:

```cpp
    QTimer *m_cursorSyncTimer;
```

Also add `QString m_pendingCursorSelector;` after the timer:

```cpp
    QString m_pendingCursorSelector;
```

This stores the selector when Preview is loading — replayed after `loadFinished`.

- [ ] **Step 2: Initialize timer in FlowTab constructor**

In `FlowTab.cpp` constructor (after line 71, `m_grabFocus(grab_focus)`), add to the initializer list:

```cpp
    m_cursorSyncTimer(nullptr),
    m_pendingCursorSelector()
```

Then in the constructor body (after line 89, `ConnectSignalsToSlots();`), add:

```cpp
    // Cursor sync debounce timer (100ms)
    m_cursorSyncTimer = new QTimer(this);
    m_cursorSyncTimer->setSingleShot(true);
    m_cursorSyncTimer->setInterval(100);
    connect(m_cursorSyncTimer, &QTimer::timeout, this, &FlowTab::syncPreviewToCursor);
```

- [ ] **Step 3: Implement `syncPreviewToCursor()` in FlowTab.cpp**

After `onCursorPositionChanged()` (after line 287):

```cpp
void FlowTab::syncPreviewToCursor()
{
    if (!m_CodeViewEditor || !m_PreviewWidget || !m_PreviewWidget->isVisible()) {
        return;
    }

    QList< ViewEditor::ElementIndex > hierarchy = m_CodeViewEditor->GetCaretLocation();
    QString selector = ElementIndexToCssSelector(hierarchy);

    if (selector.isEmpty()) {
        return;
    }

    m_PreviewWidget->scrollToCaretElement(selector);
}
```

- [ ] **Step 4: Wire `cursorPositionChanged` to debounce timer**

Modify `onCursorPositionChanged()` (line 284-287). Change from:

```cpp
void FlowTab::onCursorPositionChanged()
{
    EmitUpdateCursorPosition();
}
```

To:

```cpp
void FlowTab::onCursorPositionChanged()
{
    EmitUpdateCursorPosition();
    // Debounce cursor sync to preview
    if (m_cursorSyncTimer) {
        m_cursorSyncTimer->start();
    }
}
```

- [ ] **Step 5: Handle pending selector after preview loads**

In `ConnectSignalsToSlots()` (line 1056), after the existing PreviewWidget signal connections (after line 1081), add:

```cpp
    // When preview finishes loading, replay any pending cursor scroll
    connect(m_PreviewWidget, &PreviewWidget::loadFinished, this, [this](bool ok) {
        if (ok && !m_pendingCursorSelector.isEmpty()) {
            m_PreviewWidget->scrollToCaretElement(m_pendingCursorSelector);
            m_pendingCursorSelector.clear();
        }
    });
```

Also update `syncPreviewToCursor()` to store pending selector when preview is loading. Modify the method from Step 3:

```cpp
void FlowTab::syncPreviewToCursor()
{
    if (!m_CodeViewEditor || !m_PreviewWidget || !m_PreviewWidget->isVisible()) {
        return;
    }

    QList< ViewEditor::ElementIndex > hierarchy = m_CodeViewEditor->GetCaretLocation();
    QString selector = ElementIndexToCssSelector(hierarchy);

    if (selector.isEmpty()) {
        return;
    }

    // If preview is still loading, store selector for later
    if (!m_PreviewWidget->isVisible()) {
        m_pendingCursorSelector = selector;
        return;
    }

    m_PreviewWidget->scrollToCaretElement(selector);
}
```

Wait — `isVisible()` check and `m_isLoading` check are different concerns. Let me use a cleaner approach. The `PreviewWidget` has `m_isLoading` but it's private. Instead, let `scrollToCaretElement` itself handle the loading case.

Update `PreviewWidget::scrollToCaretElement()` from Task 1 to store the pending selector:

```cpp
void PreviewWidget::scrollToCaretElement(const QString &cssSelector)
{
    if (cssSelector.isEmpty()) {
        return;
    }

    if (m_isLoading) {
        m_pendingCaretSelector = cssSelector;
        return;
    }

    QString js = QString(
        "var element = document.querySelector('%1');"
        "if (element) {"
        "    element.scrollIntoView({behavior: 'instant', block: 'center'});"
        "}"
    ).arg(cssSelector);

    m_webView->page()->runJavaScript(js);
}
```

And in `onLoadFinished`:

```cpp
void PreviewWidget::onLoadFinished(bool ok)
{
    m_isLoading = false;
    emit loadFinished(ok);

    // Replay pending cursor scroll after load finishes
    if (ok && !m_pendingCaretSelector.isEmpty()) {
        scrollToCaretElement(m_pendingCaretSelector);
        m_pendingCaretSelector.clear();
    }
}
```

Add `QString m_pendingCaretSelector;` to the private members in `PreviewWidget.h`.

With this change, `FlowTab::syncPreviewToCursor()` stays simple (Step 3 version without pending logic), and the pending-selector handling lives entirely in PreviewWidget where `m_isLoading` is accessible.

- [ ] **Step 6: Build and verify compilation**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build.

- [ ] **Step 7: Manual test**

1. Launch Sigil, open an EPUB with multiple chapters
2. Click on a chapter tab — CodeView + Preview side by side
3. Move cursor in CodeView with arrow keys — Preview should scroll to corresponding element
4. Click at different positions in CodeView — Preview follows
5. Type quickly — Preview should not lag or flicker excessively (100ms debounce)
6. Minimize preview (drag splitter all the way) — no errors in console

- [ ] **Step 8: Commit**

```bash
git add src/Sigil/Tabs/FlowTab.h src/Sigil/Tabs/FlowTab.cpp src/Sigil/ViewEditors/PreviewWidget.h src/Sigil/ViewEditors/PreviewWidget.cpp
git commit -m "feat: add cursor sync from CodeView to Preview with element-based scrolling"
```

---

### Task 4: Edge case — CSS selector special characters

**Files:**
- Modify: `src/Sigil/Tabs/FlowTab.cpp` (sanitize selector)

- [ ] **Step 1: Add selector sanitization to `ElementIndexToCssSelector()`**

Element names from `GetCaretLocation()` come from `QXmlStreamReader::name()` which returns valid XML element names. But we should guard against edge cases (namespace prefixes, etc.). Add sanitization inside `ElementIndexToCssSelector()`:

At the start of the method, before the block-elements search, add:

```cpp
    // Sanitize element names for CSS selector safety
    // QXmlStreamReader may return namespace-prefixed names like "ns:element"
    // CSS selectors don't support colons without escaping — strip namespace prefix
    auto sanitizeName = [](const QString &name) -> QString {
        int colonPos = name.indexOf(':');
        return colonPos >= 0 ? name.mid(colonPos + 1) : name;
    };
```

Then use `sanitizeName(hierarchy[i].name)` instead of `hierarchy[i].name` in the loop and selector construction:

```cpp
    for (int i = hierarchy.size() - 1; i >= 0; --i) {
        QString name = sanitizeName(hierarchy[i].name);
        if (blockElements.contains(name)) {
            blockIndex = i;
            break;
        }
    }
```

And in the selector construction:

```cpp
    QString blockName = sanitizeName(block.name);
    QString selector = QString("%1:nth-of-type(%2)")
                           .arg(blockName)
                           .arg(block.index + 1);

    if (blockIndex + 1 < hierarchy.size()) {
        QString childName = sanitizeName(hierarchy[blockIndex + 1].name);
        selector += QString(" > %1:nth-of-type(%2)")
                        .arg(childName)
                        .arg(hierarchy[blockIndex + 1].index + 1);
    }
```

- [ ] **Step 2: Build and verify**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build.

- [ ] **Step 3: Commit**

```bash
git add src/Sigil/Tabs/FlowTab.cpp
git commit -m "fix: sanitize namespace-prefixed element names in CSS selector for cursor sync"
```

---

## Self-Review

**1. Spec coverage:**
- CodeView → Preview sync: Task 3
- Element-based mapping via GetCaretLocation(): Task 2
- 100ms debounce: Task 3
- scrollToCaretElement with JS: Task 1
- Preview loading edge case: Task 3 (pending selector in PreviewWidget)
- Preview not visible: Task 3 (isVisible check)
- No block element found: Task 2 (returns empty, syncPreviewToCursor skips)

**2. Placeholder scan:** No TBDs, TODOs, or vague steps. All code shown.

**3. Type consistency:**
- `ElementIndexToCssSelector` takes `const QList<ViewEditor::ElementIndex>&` — matches `GetCaretLocation()` return type
- `scrollToCaretElement` takes `const QString&` — matches selector output
- `m_cursorSyncTimer` is `QTimer*` — matches constructor initialization
- `m_pendingCaretSelector` is `QString` — matches `cssSelector` parameter type
