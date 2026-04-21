# User Templates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Enable user-defined templates in `~/.sigil/` for default HTML, CSS, and impressum files.

**Architecture:** New `UserTemplates` singleton in `Misc/` reads templates from `~/.sigil/` at startup. `Book` uses `UserTemplates` instead of hardcoded constants. CSS link references are adjusted to match EPUB-internal filenames.

**Tech Stack:** C++11, Qt 6 (QDir, QFile, QRegularExpression)

---

## File Structure

| File | Action | Responsibility |
|------|--------|----------------|
| `src/Sigil/Misc/UserTemplates.h` | Create | UserTemplates class declaration |
| `src/Sigil/Misc/UserTemplates.cpp` | Create | Read ~/.sigil/ files, provide templates, adjust CSS links |
| `src/Sigil/BookManipulation/Book.h` | Modify | Add `CreateImpressumFile()` declaration |
| `src/Sigil/BookManipulation/Book.cpp` | Modify | Use UserTemplates in CreateEmptyHTMLFile, CreateEmptyCSSFile, add CreateImpressumFile |
| `src/Sigil/MainUI/MainWindow.cpp` | Modify | Call CreateImpressumFile in CreateNewBook() |
| `src/Sigil/CMakeLists.txt` | Modify | Add UserTemplates.h/cpp to source list |
| `src/Sigil/sigil_constants.h` | Modify | Add IMPRESSUM_FILE_NAME constant |
| `src/Sigil/main.cpp` | Modify | Define IMPRESSUM_FILE_NAME constant |

---

### Task 1: Create UserTemplates class

**Files:**
- Create: `src/Sigil/Misc/UserTemplates.h`
- Create: `src/Sigil/Misc/UserTemplates.cpp`

- [ ] **Step 1: Write UserTemplates.h**

```cpp
#pragma once
#ifndef USER_TEMPLATES_H
#define USER_TEMPLATES_H

#include <QString>

class UserTemplates
{
public:
    static UserTemplates& instance();

    QString defaultHtml() const;
    QString defaultCss() const;
    QString impressumHtml() const;
    bool hasImpressum() const;

    QString adjustCssLinks(const QString &html, const QString &cssFilename) const;

    static QString configDir();

private:
    UserTemplates();
    QString readFile(const QString &filename) const;

    QString m_defaultHtml;
    QString m_defaultCss;
    QString m_impressumHtml;
    bool m_hasImpressum;
};

#endif
```

- [ ] **Step 2: Write UserTemplates.cpp**

```cpp
#include "UserTemplates.h"
#include "Misc/Utility.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

static const QString FALLBACK_HTML =
    "<?xml version=\"1.0\" encoding=\"utf-8\"?> "
    "<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.1//EN\" "
    "    \"http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd\">  "
    "<html xmlns=\"http://www.w3.org/1999/xhtml\"> "
    "<head> "
    "<title></title> "
    "</head> "
    "<body> "
    "<p>&#160;</p> "
    "</body> "
    "</html>";

UserTemplates &UserTemplates::instance()
{
    static UserTemplates inst;
    return inst;
}

QString UserTemplates::configDir()
{
    return QDir::homePath() + "/.sigil";
}

UserTemplates::UserTemplates()
    : m_defaultHtml(FALLBACK_HTML),
      m_defaultCss(""),
      m_impressumHtml(""),
      m_hasImpressum(false)
{
    QString dir = configDir();
    QDir config(dir);

    if (!config.exists()) {
        return;
    }

    QString html = readFile("default.xhtml");
    if (!html.isEmpty()) {
        m_defaultHtml = html;
    }

    QString css = readFile("style.css");
    if (!css.isEmpty()) {
        m_defaultCss = css;
    }

    QString impressum = readFile("impressum.xhtml");
    if (!impressum.isEmpty()) {
        m_impressumHtml = impressum;
        m_hasImpressum = true;
    }
}

QString UserTemplates::defaultHtml() const
{
    return m_defaultHtml;
}

QString UserTemplates::defaultCss() const
{
    return m_defaultCss;
}

QString UserTemplates::impressumHtml() const
{
    return m_impressumHtml;
}

bool UserTemplates::hasImpressum() const
{
    return m_hasImpressum;
}

QString UserTemplates::adjustCssLinks(const QString &html, const QString &cssFilename) const
{
    QString result = html;

    // Match href attributes in <link> tags pointing to .css files
    // e.g. href="../Styles/Style.css" -> href="../Styles/Style0001.css"
    static const QRegularExpression cssHrefRegex(
        R"((href\s*=\s*["'])([^"']*?/)([^/"']+\.css)(["']))",
        QRegularExpression::CaseInsensitiveOption);

    QList<QRegularExpressionMatch> matches;
    QRegularExpressionMatchIterator it = cssHrefRegex.globalMatch(result);
    while (it.hasNext()) {
        matches.append(it.next());
    }

    for (int i = matches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch &match = matches[i];
        QString prefix = match.captured(1);   // href="
        QString path = match.captured(2);      // ../Styles/
        QString oldName = match.captured(3);   // Style.css
        QString suffix = match.captured(4);    // "

        result.replace(match.capturedStart(), match.capturedLength(),
                       prefix + path + cssFilename + suffix);
    }

    return result;
}

QString UserTemplates::readFile(const QString &filename) const
{
    QString filepath = configDir() + "/" + filename;
    QFile file(filepath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    QString content = QString::fromUtf8(file.readAll());
    file.close();
    return content;
}
```

- [ ] **Step 3: Add UserTemplates to CMakeLists.txt**

In `src/Sigil/CMakeLists.txt`, add after the existing `Misc/SettingsStore.h` and `Misc/SettingsStore.cpp` lines (around line 239-240):

```
    Misc/UserTemplates.cpp
    Misc/UserTemplates.h
```

- [ ] **Step 4: Build and verify compilation**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build with no errors

- [ ] **Step 5: Commit**

```bash
git add src/Sigil/Misc/UserTemplates.h src/Sigil/Misc/UserTemplates.cpp src/Sigil/CMakeLists.txt
git commit -m "feat: Add UserTemplates class for ~/.sigil/ configuration"
```

---

### Task 2: Add IMPRESSUM_FILE_NAME constant

**Files:**
- Modify: `src/Sigil/sigil_constants.h`
- Modify: `src/Sigil/main.cpp`

- [ ] **Step 1: Add declaration to sigil_constants.h**

At `src/Sigil/sigil_constants.h:59` (after `FIRST_SECTION_NAME`), add:

```cpp
extern const QString IMPRESSUM_FILE_NAME;
```

- [ ] **Step 2: Add definition to main.cpp**

At `src/Sigil/main.cpp:199` (after `FIRST_SECTION_NAME` definition), add:

```cpp
const QString IMPRESSUM_FILE_NAME = "Impressum.xhtml";
```

- [ ] **Step 3: Build and verify**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -10`
Expected: Clean build

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/sigil_constants.h src/Sigil/main.cpp
git commit -m "feat: Add IMPRESSUM_FILE_NAME constant"
```

---

### Task 3: Modify Book to use UserTemplates

**Files:**
- Modify: `src/Sigil/BookManipulation/Book.h`
- Modify: `src/Sigil/BookManipulation/Book.cpp`

- [ ] **Step 1: Add CreateImpressumFile() declaration to Book.h**

At `src/Sigil/BookManipulation/Book.h:166` (after `CreateEmptyCSSFile()`), add:

```cpp
    /**
     * Creates an impressum HTML file from the user template in ~/.sigil/.
     * Only creates the file if a user template exists.
     */
    HTMLResource &CreateImpressumFile();
```

- [ ] **Step 2: Add include to Book.cpp**

At `src/Sigil/BookManipulation/Book.cpp` after the existing includes (around line 36), add:

```cpp
#include "Misc/UserTemplates.h"
```

- [ ] **Step 3: Modify CreateEmptyHTMLFile() — no-arg version**

Replace `src/Sigil/BookManipulation/Book.cpp:254-260`:

```cpp
HTMLResource &Book::CreateEmptyHTMLFile()
{
    HTMLResource &html_resource = CreateNewHTMLFile();
    html_resource.SetText(UserTemplates::instance().defaultHtml());
    SetModified(true);
    return html_resource;
}
```

- [ ] **Step 4: Modify CreateEmptyHTMLFile(HTMLResource&) — with CSS link adjustment**

Replace `src/Sigil/BookManipulation/Book.cpp:263-280`:

```cpp
HTMLResource &Book::CreateEmptyHTMLFile(HTMLResource &resource)
{
    HTMLResource &new_resource = CreateNewHTMLFile();

    QString html = UserTemplates::instance().defaultHtml();

    // Adjust CSS links to match the EPUB-internal stylesheet filename
    QList< CSSResource* > css_resources = m_Mainfolder.GetResourceTypeList< CSSResource >(false);
    if (!css_resources.isEmpty()) {
        QString cssFilename = css_resources.first()->Filename();
        html = UserTemplates::instance().adjustCssLinks(html, cssFilename);
    }

    new_resource.SetText(html);

    if (&resource != NULL) {
        QList< HTMLResource * > html_resources = m_Mainfolder.GetResourceTypeList< HTMLResource >(true);
        int reading_order = GetOPF().GetReadingOrder(resource) + 1;

        if (reading_order > 0) {
            html_resources.insert(reading_order, &new_resource);
            GetOPF().UpdateSpineOrder(html_resources);
        }
    }

    SetModified(true);
    return new_resource;
}
```

Note: The no-arg version does NOT adjust CSS links because when it's called (during new book creation), no CSS resource exists yet. The CSS is created after, and the initial HTML link will be correct if the user's template uses `Style.css` as the filename — which gets auto-renamed to `Style0001.css` by FolderKeeper. The `adjustCssLinks()` method handles the rename. However, since the CSS file doesn't exist yet at this point, the link stays as-is and the CSS filename matching happens naturally through FolderKeeper's `GetUniqueFilenameVersion()`.

Actually, let me reconsider. When `CreateNewBook()` calls:
1. `CreateEmptyHTMLFile()` — HTML created first
2. Then CSS created separately

The HTML's `../Styles/Style.css` link will NOT match `Style0001.css`. So we DO need adjustment, but we need to do it after CSS creation. This is better handled in `MainWindow::CreateNewBook()` after all files are created (see Task 4).

- [ ] **Step 5: Modify CreateEmptyCSSFile()**

Replace `src/Sigil/BookManipulation/Book.cpp:338-347`:

```cpp
CSSResource &Book::CreateEmptyCSSFile()
{
    TempFolder tempfolder;
    QString fullfilepath = tempfolder.GetPath() + "/" + m_Mainfolder.GetUniqueFilenameVersion(FIRST_CSS_NAME);
    Utility::WriteUnicodeTextFile(UserTemplates::instance().defaultCss(), fullfilepath);
    CSSResource &css_resource = *qobject_cast< CSSResource * >(
                                    &m_Mainfolder.AddContentFileToFolder(fullfilepath));
    SetModified(true);
    return css_resource;
}
```

- [ ] **Step 6: Add CreateImpressumFile() implementation**

Add after `CreateEmptyHTMLFile(HTMLResource &resource)` in Book.cpp:

```cpp
HTMLResource &Book::CreateImpressumFile()
{
    TempFolder tempfolder;
    QString fullfilepath = tempfolder.GetPath() + "/" + IMPRESSUM_FILE_NAME;
    Utility::WriteUnicodeTextFile(PLACEHOLDER_TEXT, fullfilepath);
    HTMLResource &html_resource = *qobject_cast< HTMLResource * >(
                                      &m_Mainfolder.AddContentFileToFolder(fullfilepath));

    QString html = UserTemplates::instance().impressumHtml();

    // Adjust CSS links to match EPUB-internal stylesheet filename
    QList< CSSResource* > css_resources = m_Mainfolder.GetResourceTypeList< CSSResource >(false);
    if (!css_resources.isEmpty()) {
        QString cssFilename = css_resources.first()->Filename();
        html = UserTemplates::instance().adjustCssLinks(html, cssFilename);
    }

    html_resource.SetText(html);
    SetModified(true);
    return html_resource;
}
```

- [ ] **Step 7: Build and verify**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build (may have unused variable warning for EMPTY_HTML_FILE — that's fine, we'll clean it up later)

- [ ] **Step 8: Commit**

```bash
git add src/Sigil/BookManipulation/Book.h src/Sigil/BookManipulation/Book.cpp
git commit -m "feat: Book uses UserTemplates for HTML, CSS, and impressum creation"
```

---

### Task 4: Integrate impressum into new book creation

**Files:**
- Modify: `src/Sigil/MainUI/MainWindow.cpp`

- [ ] **Step 1: Add UserTemplates include**

At `src/Sigil/MainUI/MainWindow.cpp` in the includes section, add:

```cpp
#include "Misc/UserTemplates.h"
```

- [ ] **Step 2: Modify CreateNewBook()**

Replace `src/Sigil/MainUI/MainWindow.cpp:3119-3127`:

```cpp
void MainWindow::CreateNewBook()
{
    QSharedPointer< Book > new_book = QSharedPointer< Book >(new Book());

    // Create CSS first so HTML files can reference it
    CSSResource &css = new_book->CreateEmptyCSSFile();

    // Create impressum as first page if user has a template
    if (UserTemplates::instance().hasImpressum()) {
        new_book->CreateImpressumFile();
    }

    // Create first section with default HTML template
    HTMLResource &first_section = new_book->CreateEmptyHTMLFile();

    // Adjust CSS links in all HTML files to match actual CSS filename
    QString cssFilename = css.Filename();
    QList< HTMLResource* > html_resources = new_book->GetHTMLResources();
    for (HTMLResource *resource : html_resources) {
        QString html = resource->GetText();
        QString adjusted = UserTemplates::instance().adjustCssLinks(html, cssFilename);
        if (html != adjusted) {
            resource->SetText(adjusted);
        }
    }

    SetNewBook(new_book);
    new_book->SetModified(false);
    m_SaveACopyFilename = "";
    UpdateUiWithCurrentFile("");
}
```

- [ ] **Step 3: Build and verify**

Run: `cd /data1/u2/sigil074/build && make -j4 2>&1 | tail -20`
Expected: Clean build

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/MainUI/MainWindow.cpp
git commit -m "feat: Create impressum and apply user templates in new book"
```

---

### Task 5: Manual testing

- [ ] **Step 1: Test without ~/.sigil/** (fallback behavior)

1. Ensure `~/.sigil/` does NOT exist
2. Start Sigil, create new book
3. Verify: empty HTML with `&#160;`, empty CSS, no impressum
4. Verify: no crash, no error messages

- [ ] **Step 2: Test with ~/.sigil/ templates**

1. Create `~/.sigil/` directory
2. Copy `default.xhtml`, `style.css`, and `impressum.xhtml` from `build/bin/default-summary.txt` content
3. Start Sigil, create new book
4. Verify: HTML has your template content with CSS link
5. Verify: CSS has your stylesheet content
6. Verify: Impressum.xhtml appears as first page in BookBrowser
7. Verify: CSS link in HTML points to actual filename (e.g. `Style0001.css`)

- [ ] **Step 3: Test partial config**

1. Keep only `style.css` in `~/.sigil/`, remove other two
2. Create new book
3. Verify: default HTML (fallback), your CSS, no impressum

- [ ] **Step 4: Commit final state**

```bash
git add -A
git commit -m "feat: User templates from ~/.sigil/ with impressum support"
```

---

### Task 6: Update RETROSPECTIVE.md

**Files:**
- Modify: `docs/superpowers/RETROSPECTIVE.md`

- [ ] **Step 1: Add Phase 3 section to RETROSPECTIVE.md**

Add after the Phase 2 section, covering the UserTemplates feature addition.

- [ ] **Step 2: Commit**

```bash
git add docs/superpowers/RETROSPECTIVE.md
git commit -m "docs: Update retrospective with UserTemplates feature"
```
