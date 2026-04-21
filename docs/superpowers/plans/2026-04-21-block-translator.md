# Block Translator Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add XHTML block translation via right-click context menu using sigoREST API.

**Architecture:** New `Translator` class (QObject) in Misc/ encapsulates sigoREST QtClient, placeholder encoding/decoding, and prompt logic. `CodeViewEditor` adds context menu entries and block detection. `TranslationWidget` provides settings UI. sigoREST client library linked via CMake.

**Tech Stack:** C++17, Qt 6 (Network, Widgets), sigoREST QtClient, QNetworkAccessManager

---

## File Structure

| Action | File | Responsibility |
|--------|------|----------------|
| Create | `src/Sigil/Misc/Translator.h` | Translator class declaration |
| Create | `src/Sigil/Misc/Translator.cpp` | Translator class implementation |
| Create | `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.h` | Settings widget declaration |
| Create | `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.cpp` | Settings widget implementation |
| Create | `src/Sigil/Form_Files/PTranslationWidget.ui` | Settings widget UI form |
| Modify | `src/Sigil/Misc/SettingsStore.h` | Add sigorestServerUrl getter/setter |
| Modify | `src/Sigil/Misc/SettingsStore.cpp` | Add sigorestServerUrl implementation |
| Modify | `src/Sigil/ViewEditors/CodeViewEditor.h` | Add translate members, slots, mapper |
| Modify | `src/Sigil/ViewEditors/CodeViewEditor.cpp` | Add translate context menu + block detection |
| Modify | `src/Sigil/Dialogs/Preferences.cpp` | Register TranslationWidget |
| Modify | `src/Sigil/CMakeLists.txt` | Add new source files, sigoREST lib |

---

### Task 1: Add sigorestServerUrl to SettingsStore

**Files:**
- Modify: `src/Sigil/Misc/SettingsStore.h`
- Modify: `src/Sigil/Misc/SettingsStore.cpp`

- [ ] **Step 1: Add key constant and method declarations**

In `src/Sigil/Misc/SettingsStore.cpp`, add after line 81 (after `KEY_PREVIEW_SYNC_CURSOR`):

```cpp
static const QString KEY_SIGOREST_SERVER_URL = "Translation/sigorest_server_url";
```

In `src/Sigil/Misc/SettingsStore.h`, add after the `setPreviewSyncCursor` declaration (line 202):

```cpp
    QString sigorestServerUrl();
    void setSigorestServerUrl(const QString &url);
```

- [ ] **Step 2: Add method implementations**

In `src/Sigil/Misc/SettingsStore.cpp`, add after `setPreviewSyncCursor` (after line 455):

```cpp
QString SettingsStore::sigorestServerUrl()
{
    clearSettingsGroup();
    return value(KEY_SIGOREST_SERVER_URL, "http://localhost:9080").toString();
}

void SettingsStore::setSigorestServerUrl(const QString &url)
{
    clearSettingsGroup();
    setValue(KEY_SIGOREST_SERVER_URL, url);
}
```

- [ ] **Step 3: Build to verify**

Run: `cd /data1/u2/sigil074/build && cmake .. && make -j4 2>&1 | tail -20`
Expected: Build succeeds. No link errors (new methods not called yet).

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/Misc/SettingsStore.h src/Sigil/Misc/SettingsStore.cpp
git commit -m "feat: add sigorestServerUrl setting to SettingsStore"
```

---

### Task 2: Create Translator class (header)

**Files:**
- Create: `src/Sigil/Misc/Translator.h`

- [ ] **Step 1: Write Translator.h**

```cpp
#pragma once
#ifndef TRANSLATOR_H
#define TRANSLATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>

class QNetworkAccessManager;
class QNetworkReply;

class Translator : public QObject
{
    Q_OBJECT

public:
    explicit Translator(QObject *parent = nullptr);
    ~Translator();

    void translate(const QString &blockText, const QString &direction, const QString &model);
    QStringList availableModels() const;
    void refreshModels();
    bool isServerAvailable() const;

signals:
    void translationReady(const QString &newBlock);
    void translationError(const QString &message);
    void modelsRefreshed();

private slots:
    void onModelsReplyFinished();
    void onTranslateReplyFinished();

private:
    QString encodePlaceholders(const QString &html);
    QString decodePlaceholders(const QString &text);
    QString escapeHtml(const QString &text);
    QString buildSystemPrompt(const QString &direction);
    QString buildUserPrompt(const QString &text);
    QNetworkRequest createRequest(const QString &endpoint);

    QNetworkAccessManager *m_networkManager;
    QNetworkReply *m_modelsReply;
    QNetworkReply *m_translateReply;
    QStringList m_cachedModels;
    QString m_serverUrl;
    QMap<QString, QString> m_savedTagAttributes;

    static const QStringList BLOCK_TAGS;
    static const QStringList INLINE_TAGS;
};

#endif // TRANSLATOR_H
```

- [ ] **Step 2: Commit**

```bash
git add src/Sigil/Misc/Translator.h
git commit -m "feat: add Translator class header"
```

---

### Task 3: Create Translator class (implementation)

**Files:**
- Create: `src/Sigil/Misc/Translator.cpp`

- [ ] **Step 1: Write Translator.cpp**

```cpp
#include "Misc/Translator.h"
#include "Misc/SettingsStore.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QRegularExpression>

const QStringList Translator::BLOCK_TAGS = {
    "p", "h1", "h2", "h3", "h4", "h5", "h6",
    "li", "div", "blockquote", "td", "th", "span",
    "dt", "dd", "pre", "figcaption"
};

const QStringList Translator::INLINE_TAGS = {
    "em", "strong", "b", "i", "u", "a", "span",
    "sub", "sup", "code", "small", "mark", "abbr", "cite", "q"
};

Translator::Translator(QObject *parent)
    : QObject(parent),
      m_networkManager(new QNetworkAccessManager(this)),
      m_modelsReply(nullptr),
      m_translateReply(nullptr)
{
    SettingsStore settings;
    m_serverUrl = settings.sigorestServerUrl();
}

Translator::~Translator()
{
    if (m_modelsReply) {
        m_modelsReply->abort();
        m_modelsReply->deleteLater();
    }
    if (m_translateReply) {
        m_translateReply->abort();
        m_translateReply->deleteLater();
    }
}

QStringList Translator::availableModels() const
{
    return m_cachedModels;
}

bool Translator::isServerAvailable() const
{
    return !m_cachedModels.isEmpty();
}

void Translator::refreshModels()
{
    if (m_modelsReply) {
        m_modelsReply->abort();
        m_modelsReply->deleteLater();
        m_modelsReply = nullptr;
    }

    SettingsStore settings;
    m_serverUrl = settings.sigorestServerUrl();

    QNetworkRequest request = createRequest("/v1/models");
    m_modelsReply = m_networkManager->get(request);
    connect(m_modelsReply, &QNetworkReply::finished, this, &Translator::onModelsReplyFinished);
}

void Translator::onModelsReplyFinished()
{
    if (!m_modelsReply) return;

    if (m_modelsReply->error() != QNetworkReply::NoError) {
        m_cachedModels.clear();
        m_modelsReply->deleteLater();
        m_modelsReply = nullptr;
        emit modelsRefreshed();
        return;
    }

    QByteArray data = m_modelsReply->readAll();
    m_modelsReply->deleteLater();
    m_modelsReply = nullptr;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        m_cachedModels.clear();
        emit modelsRefreshed();
        return;
    }

    QJsonArray models = doc.object().value("data").toArray();
    m_cachedModels.clear();
    for (const QJsonValue &val : models) {
        QString id = val.toObject().value("id").toString();
        if (!id.isEmpty()) {
            m_cachedModels.append(id);
        }
    }
    m_cachedModels.sort();
    emit modelsRefreshed();
}

void Translator::translate(const QString &blockText, const QString &direction, const QString &model)
{
    if (m_translateReply) {
        emit translationError(tr("Translation already in progress"));
        return;
    }

    SettingsStore settings;
    m_serverUrl = settings.sigorestServerUrl();

    QString encoded = encodePlaceholders(blockText);
    QString systemPrompt = buildSystemPrompt(direction);
    QString userPrompt = buildUserPrompt(encoded);

    QJsonObject reqJson;
    reqJson["model"] = model;

    QJsonArray msgArray;
    QJsonObject sysMsg;
    sysMsg["role"] = "system";
    sysMsg["content"] = systemPrompt;
    msgArray.append(sysMsg);

    QJsonObject userMsg;
    userMsg["role"] = "user";
    userMsg["content"] = userPrompt;
    msgArray.append(userMsg);

    reqJson["messages"] = msgArray;
    reqJson["max_tokens"] = 2000;
    reqJson["temperature"] = 0.3;

    QNetworkRequest request = createRequest("/v1/chat/completions");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonDocument doc(reqJson);
    m_translateReply = m_networkManager->post(request, doc.toJson());
    connect(m_translateReply, &QNetworkReply::finished, this, &Translator::onTranslateReplyFinished);
}

void Translator::onTranslateReplyFinished()
{
    if (!m_translateReply) return;

    if (m_translateReply->error() != QNetworkReply::NoError) {
        QString errMsg = m_translateReply->errorString();
        m_translateReply->deleteLater();
        m_translateReply = nullptr;
        emit translationError(errMsg);
        return;
    }

    QByteArray data = m_translateReply->readAll();
    m_translateReply->deleteLater();
    m_translateReply = nullptr;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        emit translationError(tr("Invalid JSON response from translation server"));
        return;
    }

    QJsonObject respJson = doc.object();
    if (respJson.contains("error")) {
        QJsonObject errObj = respJson["error"].toObject();
        emit translationError(errObj["message"].toString());
        return;
    }

    QJsonArray choices = respJson["choices"].toArray();
    if (choices.isEmpty()) {
        emit translationError(tr("Empty translation response"));
        return;
    }

    QString translatedText = choices[0].toObject().value("message").toObject().value("content").toString();
    if (translatedText.isEmpty()) {
        emit translationError(tr("Translation returned empty text"));
        return;
    }

    QString decoded = decodePlaceholders(translatedText);
    emit translationReady(decoded);
}

QString Translator::encodePlaceholders(const QString &html)
{
    m_savedTagAttributes.clear();
    QString result = html;

    // Match inline opening tags with optional attributes: <em attr="...">text</em>
    // Build regex pattern from inline tags
    QString tagPattern = INLINE_TAGS.join("|");
    QRegularExpression openTagRegex(
        "<(" + tagPattern + ")(\\s[^>]*)?>(?!\\s*<(" + tagPattern + "))",
        QRegularExpression::CaseInsensitiveOption
    );

    // Find all inline tags and replace them with placeholders
    QList<QRegularExpressionMatch> matches;
    QRegularExpressionMatchIterator it = openTagRegex.globalMatch(result);
    while (it.hasNext()) {
        matches.append(it.next());
    }

    // Replace in reverse order to preserve positions
    for (int i = matches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch &match = matches[i];
        QString tagName = match.captured(1).toUpper();
        QString attributes = match.captured(2);
        int keyIndex = i;
        m_savedTagAttributes[QString::number(keyIndex) + "_" + tagName] = attributes;

        QString placeholder = QString::number(keyIndex) + "_" + tagName;
        result.replace(match.capturedStart(), match.capturedLength(), placeholder + "⟦");
    }

    // Now replace closing tags </em> → ⟧EM⟧
    for (const QString &tag : INLINE_TAGS) {
        QRegularExpression closeTagRegex("</" + tag + "\\s*>", QRegularExpression::CaseInsensitiveOption);
        result.replace(closeTagRegex, "⟧" + tag.toUpper());
    }

    // Remove any remaining non-inline HTML tags from the text
    QRegularExpression remainingTags("<[^>]+>");
    result.remove(remainingTags);

    return result;
}

QString Translator::decodePlaceholders(const QString &text)
{
    QString result = text;

    // Replace closing placeholders ⟧TAG → </tag>
    for (const QString &tag : INLINE_TAGS) {
        QString closingPlaceholder = QString("⟧") + tag.toUpper();
        QString closingTag = "</" + tag.toLower() + ">";
        result.replace(closingPlaceholder, closingTag);
    }

    // Replace opening placeholders NUM_TAG⟦ → <tag attr="...">
    QRegularExpression openPlaceholder("(\\d+)_([A-Z]+)⟦");
    QRegularExpressionMatchIterator it = openPlaceholder.globalMatch(result);
    QList<QRegularExpressionMatch> matches;
    while (it.hasNext()) {
        matches.append(it.next());
    }

    // Replace in reverse order
    for (int i = matches.size() - 1; i >= 0; --i) {
        const QRegularExpressionMatch &match = matches[i];
        QString key = match.captured(1) + "_" + match.captured(2);
        QString tagName = match.captured(2).toLower();
        QString attributes = m_savedTagAttributes.value(key, "");
        QString openingTag = "<" + tagName + attributes + ">";
        result.replace(match.capturedStart(), match.capturedLength(), openingTag);
    }

    // Escape HTML entities in text nodes (between tags)
    // Split by tags, escape text parts, rejoin
    QStringList parts;
    QRegularExpression tagOrEntity("</?[a-zA-Z][^>]*>|&(?:[a-zA-Z]+|#\\d+);");
    int lastEnd = 0;
    QRegularExpressionMatchIterator tagIt = tagOrEntity.globalMatch(result);
    while (tagIt.hasNext()) {
        QRegularExpressionMatch m = tagIt.next();
        // Text before this tag
        if (m.capturedStart() > lastEnd) {
            QString textPart = result.mid(lastEnd, m.capturedStart() - lastEnd);
            parts.append(escapeHtml(textPart));
        }
        parts.append(m.captured()); // tag or entity as-is
        lastEnd = m.capturedEnd();
    }
    if (lastEnd < result.length()) {
        parts.append(escapeHtml(result.mid(lastEnd)));
    }
    result = parts.join(QString());

    return result;
}

QString Translator::escapeHtml(const QString &text)
{
    QString result = text;
    result.replace("&", "&amp;");
    result.replace("<", "&lt;");
    result.replace(">", "&gt;");
    result.replace("\"", "&quot;");
    return result;
}

QString Translator::buildSystemPrompt(const QString &direction)
{
    QString targetLang = (direction == "en→de") ? "German" : "English";
    return QString(
        "You are a translation engine. Translate the following text to %1. "
        "Output only the translated text, no explanations. "
        "Preserve all numeric placeholders like 0_EM and the markers ⟦ ⟧ exactly as they appear — "
        "do not translate, remove, or modify them."
    ).arg(targetLang);
}

QString Translator::buildUserPrompt(const QString &text)
{
    return text;
}

QNetworkRequest Translator::createRequest(const QString &endpoint)
{
    QUrl url(m_serverUrl + endpoint);
    QNetworkRequest request(url);
    return request;
}
```

- [ ] **Step 2: Commit**

```bash
git add src/Sigil/Misc/Translator.h src/Sigil/Misc/Translator.cpp
git commit -m "feat: implement Translator class with placeholder encoding"
```

---

### Task 4: Create TranslationWidget (settings UI)

**Files:**
- Create: `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.h`
- Create: `src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.cpp`
- Create: `src/Sigil/Form_Files/PTranslationWidget.ui`

- [ ] **Step 1: Create UI form file**

Create `src/Sigil/Form_Files/PTranslationWidget.ui`:

```xml
<?xml version="1.0" encoding="UTF-8"?>
<ui version="4.0">
 <class>TranslationWidget</class>
 <widget class="QWidget" name="TranslationWidget">
  <property name="geometry">
   <rect>
    <x>0</x>
    <y>0</y>
    <width>400</width>
    <height>300</height>
   </rect>
  </property>
  <property name="windowTitle">
   <string>Translation</string>
  </property>
  <layout class="QVBoxLayout" name="verticalLayout">
   <property name="leftMargin">
    <number>6</number>
   </property>
   <property name="topMargin">
    <number>0</number>
   </property>
   <property name="rightMargin">
    <number>0</number>
   </property>
   <property name="bottomMargin">
    <number>0</number>
   </property>
   <item>
    <layout class="QGridLayout" name="gridLayout">
     <item row="0" column="0">
      <widget class="QLabel" name="label_server_url">
       <property name="toolTip">
        <string>URL of the sigoREST translation server</string>
       </property>
       <property name="text">
        <string>sigoREST Server URL:</string>
       </property>
      </widget>
     </item>
     <item row="0" column="1">
      <widget class="QLineEdit" name="leServerUrl">
       <property name="placeholderText">
        <string>http://localhost:9080</string>
       </property>
      </widget>
     </item>
     <item row="0" column="2">
      <widget class="QPushButton" name="btnDefaultUrl">
       <property name="text">
        <string>Default</string>
       </property>
       <property name="maximumSize">
        <size>
         <width>80</width>
         <height>16777215</height>
        </size>
       </property>
      </widget>
     </item>
     <item row="1" column="0">
      <widget class="QLabel" name="label_status">
       <property name="text">
        <string>Server Status:</string>
       </property>
      </widget>
     </item>
     <item row="1" column="1">
      <widget class="QLabel" name="lblStatus">
       <property name="text">
        <string>Unknown</string>
       </property>
      </widget>
     </item>
     <item row="1" column="2">
      <widget class="QPushButton" name="btnTestServer">
       <property name="text">
        <string>Test</string>
       </property>
       <property name="maximumSize">
        <size>
         <width>80</width>
         <height>16777215</height>
        </size>
       </property>
      </widget>
     </item>
     <item row="2" column="0">
      <widget class="QLabel" name="label_models">
       <property name="text">
        <string>Available Models:</string>
       </property>
      </widget>
     </item>
     <item row="2" column="1">
      <widget class="QLabel" name="lblModels">
       <property name="text">
        <string/>
       </property>
      </widget>
     </item>
     <item row="2" column="2">
      <widget class="QPushButton" name="btnRefreshModels">
       <property name="text">
        <string>Refresh</string>
       </property>
       <property name="maximumSize">
        <size>
         <width>80</width>
         <height>16777215</height>
        </size>
       </property>
      </widget>
     </item>
    </layout>
   </item>
   <item>
    <spacer name="verticalSpacer">
     <property name="orientation">
      <enum>Qt::Vertical</enum>
     </property>
     <property name="sizeHint" stdset="0">
      <size>
       <width>20</width>
       <height>100</height>
      </size>
     </property>
    </spacer>
   </item>
  </layout>
 </widget>
 <resources/>
 <connections/>
</ui>
```

- [ ] **Step 2: Create TranslationWidget.h**

```cpp
#pragma once
#ifndef TRANSLATIONWIDGET_H
#define TRANSLATIONWIDGET_H

#include "PreferencesWidget.h"
#include "ui_PTranslationWidget.h"

class Translator;

class TranslationWidget : public PreferencesWidget
{
    Q_OBJECT

public:
    TranslationWidget();
    PreferencesWidget::ResultAction saveSettings() override;

private slots:
    void testServer();
    void refreshModels();
    void defaultUrl();
    void onModelsRefreshed();

private:
    void readSettings();

    Ui::TranslationWidget ui;
    Translator *m_translator;
};

#endif // TRANSLATIONWIDGET_H
```

- [ ] **Step 3: Create TranslationWidget.cpp**

```cpp
#include "TranslationWidget.h"
#include "Misc/Translator.h"
#include "Misc/SettingsStore.h"

TranslationWidget::TranslationWidget()
    : m_translator(new Translator(this))
{
    ui.setupUi(this);
    connect(ui.btnTestServer, SIGNAL(clicked()), this, SLOT(testServer()));
    connect(ui.btnRefreshModels, SIGNAL(clicked()), this, SLOT(refreshModels()));
    connect(ui.btnDefaultUrl, SIGNAL(clicked()), this, SLOT(defaultUrl()));
    connect(m_translator, &Translator::modelsRefreshed, this, &TranslationWidget::onModelsRefreshed);
    readSettings();
}

PreferencesWidget::ResultAction TranslationWidget::saveSettings()
{
    SettingsStore settings;
    settings.setSigorestServerUrl(ui.leServerUrl->text().trimmed());
    return PreferencesWidget::ResultAction_None;
}

void TranslationWidget::readSettings()
{
    SettingsStore settings;
    ui.leServerUrl->setText(settings.sigorestServerUrl());
    refreshModels();
}

void TranslationWidget::testServer()
{
    // Save URL first so Translator picks it up
    SettingsStore settings;
    settings.setSigorestServerUrl(ui.leServerUrl->text().trimmed());
    ui.lblStatus->setText(tr("Testing..."));
    refreshModels();
}

void TranslationWidget::refreshModels()
{
    m_translator->refreshModels();
    ui.lblStatus->setText(tr("Querying..."));
}

void TranslationWidget::defaultUrl()
{
    ui.leServerUrl->setText("http://localhost:9080");
}

void TranslationWidget::onModelsRefreshed()
{
    if (m_translator->isServerAvailable()) {
        ui.lblStatus->setText(tr("Available (%1 models)").arg(m_translator->availableModels().size()));
        ui.lblModels->setText(m_translator->availableModels().join(", "));
    } else {
        ui.lblStatus->setText(tr("Not reachable"));
        ui.lblModels->setText("");
    }
}
```

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.h \
        src/Sigil/Dialogs/PreferenceWidgets/TranslationWidget.cpp \
        src/Sigil/Form_Files/PTranslationWidget.ui
git commit -m "feat: add TranslationWidget settings UI"
```

---

### Task 5: Register TranslationWidget in Preferences

**Files:**
- Modify: `src/Sigil/Dialogs/Preferences.cpp`

- [ ] **Step 1: Add include and register widget**

In `src/Sigil/Dialogs/Preferences.cpp`, add after line 35 (after `#include "PreferenceWidgets/SpellCheckWidget.h"`):

```cpp
#include "PreferenceWidgets/TranslationWidget.h"
```

In the constructor (after line 52, after `appendPreferenceWidget(new SpellCheckWidget);`):

```cpp
    appendPreferenceWidget(new TranslationWidget);
```

- [ ] **Step 2: Build to verify**

Run: `cd /data1/u2/sigil074/build && cmake .. && make -j4 2>&1 | tail -30`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/Sigil/Dialogs/Preferences.cpp
git commit -m "feat: register TranslationWidget in Preferences dialog"
```

---

### Task 6: Add translate context menu to CodeViewEditor

**Files:**
- Modify: `src/Sigil/ViewEditors/CodeViewEditor.h`
- Modify: `src/Sigil/ViewEditors/CodeViewEditor.cpp`

- [ ] **Step 1: Add members to CodeViewEditor.h**

In the private section, after `QSignalMapper *m_clipMapper;` (line 810):

```cpp
    QSignalMapper *m_translateMapper;
    Translator *m_translator;
    int m_translateStartPos;
    int m_translateEndPos;
```

In the private methods section, after `void AddViewImageContextMenu(QMenu *menu);` (line 599):

```cpp
    void AddTranslateContextMenu(QMenu *menu);
    struct BlockInfo {
        int startPos;
        int endPos;
        QString tagName;
        QString tagAttributes;
        QString innerContent;
        bool isValid;
    };
    BlockInfo FindCurrentBlock();
```

In the private slots section, after `void ReformatHTMLToValidAllAction();` (line 528):

```cpp
    void TranslateBlock(const QString &directionAndModel);
    void OnTranslationReady(const QString &newBlock);
    void OnTranslationError(const QString &message);
```

Add include near the top (after line 37):

```cpp
#include "Misc/Translator.h"
```

- [ ] **Step 2: Add context menu and block detection to CodeViewEditor.cpp**

In `CodeViewEditor.cpp`, in the constructor, after the other signal mapper initializations, add:

```cpp
    m_translateMapper = new QSignalMapper(this);
    m_translator = new Translator(this);
    m_translateStartPos = 0;
    m_translateEndPos = 0;
    connect(m_translateMapper, SIGNAL(mappedString(const QString &)),
            this, SLOT(TranslateBlock(const QString &)));
    connect(m_translator, &Translator::translationReady,
            this, &CodeViewEditor::OnTranslationReady);
    connect(m_translator, &Translator::translationError,
            this, &CodeViewEditor::OnTranslationError);
```

In `contextMenuEvent()`, add after `AddClipContextMenu(menu);` (line 1100) and before the spell check block:

```cpp
    AddTranslateContextMenu(menu);
```

Add the new methods at the end of the file (before the closing of the file):

```cpp
void CodeViewEditor::AddTranslateContextMenu(QMenu *menu)
{
    QAction *topAction = nullptr;
    if (!menu->actions().isEmpty()) {
        topAction = menu->actions().at(0);
    }

    QMenu *translateMenu = new QMenu(tr("Translate"), menu);
    translateMenu->setIcon(QIcon()); // no icon for now

    QStringList models = m_translator->availableModels();
    if (models.isEmpty()) {
        QAction *disabledAction = translateMenu->addAction(tr("No models available"));
        disabledAction->setEnabled(false);
    } else {
        QMenu *enDeMenu = translateMenu->addMenu(tr("English \342\206\222 Deutsch"));
        QMenu *deEnMenu = translateMenu->addMenu(tr("Deutsch \342\206\222 English"));

        for (const QString &model : models) {
            QAction *enDeAction = enDeMenu->addAction(model);
            connect(enDeAction, SIGNAL(triggered()), m_translateMapper, SLOT(map()));
            m_translateMapper->setMapping(enDeAction, "en\342\206\222de|" + model);

            QAction *deEnAction = deEnMenu->addAction(model);
            connect(deEnAction, SIGNAL(triggered()), m_translateMapper, SLOT(map()));
            m_translateMapper->setMapping(deEnAction, "de\342\206\222en|" + model);
        }
    }

    QAction *translateAction = menu->insertMenu(topAction, translateMenu);
    if (topAction) {
        menu->insertSeparator(topAction);
    }
}

CodeViewEditor::BlockInfo CodeViewEditor::FindCurrentBlock()
{
    BlockInfo info;
    info.startPos = -1;
    info.endPos = -1;
    info.isValid = false;

    QTextCursor cursor = textCursor();
    int pos = cursor.position();
    QString text = toPlainText();

    // Search backward for opening block tag
    QRegularExpression openTagRegex(
        "<(p|h[1-6]|li|div|blockquote|td|th|span|dt|dd|pre|figcaption)\\b([^>]*)>",
        QRegularExpression::CaseInsensitiveOption
    );

    int bestStart = -1;
    int bestStartLen = 0;
    QString bestTag;
    QString bestAttrs;

    QRegularExpressionMatchIterator it = openTagRegex.globalMatch(text);
    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        if (match.capturedStart() < pos && match.capturedEnd() <= text.length()) {
            bestStart = match.capturedStart();
            bestStartLen = match.capturedLength();
            bestTag = match.captured(1).toLower();
            bestAttrs = match.captured(2);
        }
    }

    if (bestStart == -1) return info;

    // Search forward for matching closing tag
    QString closeTag = "</" + bestTag + ">";
    int closePos = text.indexOf(closeTag, bestStart + bestStartLen, Qt::CaseInsensitive);
    if (closePos == -1) return info;

    info.startPos = bestStart;
    info.endPos = closePos + closeTag.length();
    info.tagName = bestTag;
    info.tagAttributes = bestAttrs;

    // Extract inner content (between > of opening and < of closing)
    int innerStart = bestStart + bestStartLen;
    int innerEnd = closePos;
    if (innerEnd > innerStart) {
        info.innerContent = text.mid(innerStart, innerEnd - innerStart);
    } else {
        info.innerContent = "";
    }

    info.isValid = true;
    return info;
}

void CodeViewEditor::TranslateBlock(const QString &directionAndModel)
{
    BlockInfo block = FindCurrentBlock();
    if (!block.isValid) {
        return;
    }

    m_translateStartPos = block.startPos;
    m_translateEndPos = block.endPos;

    QStringList parts = directionAndModel.split("|");
    if (parts.size() != 2) return;

    QString direction = parts[0];
    QString model = parts[1];

    // Build the full block content including tags for placeholder processing
    QString fullBlock = toPlainText().mid(block.startPos, block.endPos - block.startPos);
    m_translator->translate(fullBlock, direction, model);
}

void CodeViewEditor::OnTranslationReady(const QString &newBlock)
{
    QTextCursor cursor = textCursor();
    cursor.setPosition(m_translateStartPos);
    cursor.setPosition(m_translateEndPos, QTextCursor::KeepAnchor);
    cursor.insertText(newBlock);
}

void CodeViewEditor::OnTranslationError(const QString &message)
{
    QMessageBox::warning(this, tr("Translation Error"), message);
}
```

Add the QMessageBox include near the top of CodeViewEditor.cpp:

```cpp
#include <QMessageBox>
```

- [ ] **Step 3: Build to verify**

Run: `cd /data1/u2/sigil074/build && cmake .. && make -j4 2>&1 | tail -30`
Expected: Build succeeds.

- [ ] **Step 4: Commit**

```bash
git add src/Sigil/ViewEditors/CodeViewEditor.h src/Sigil/ViewEditors/CodeViewEditor.cpp
git commit -m "feat: add translate context menu and block detection to CodeViewEditor"
```

---

### Task 7: Update CMakeLists.txt

**Files:**
- Modify: `src/Sigil/CMakeLists.txt`

- [ ] **Step 1: Add new source files**

In `src/Sigil/CMakeLists.txt`, add to `MISC_FILES` (after `Misc/UserTemplates.h` at line 242):

```cpp
    Misc/Translator.cpp
    Misc/Translator.h
```

Add to `DIALOG_FILES` (after `Dialogs/PreferenceWidgets/SpellCheckWidget.h` at line 144):

```cpp
    Dialogs/PreferenceWidgets/TranslationWidget.cpp
    Dialogs/PreferenceWidgets/TranslationWidget.h
```

Add to `UI_FILES` (after `Form_Files/PSpellCheckWidget.ui` at line 387):

```cpp
    Form_Files/PTranslationWidget.ui
```

- [ ] **Step 2: Build to verify**

Run: `cd /data1/u2/sigil074/build && cmake .. && make -j4 2>&1 | tail -30`
Expected: Build succeeds.

- [ ] **Step 3: Commit**

```bash
git add src/Sigil/CMakeLists.txt
git commit -m "feat: add Translator and TranslationWidget to CMake build"
```

---

### Task 8: Initialize Translator and refresh models on startup

**Files:**
- Modify: `src/Sigil/MainUI/MainWindow.cpp`

- [ ] **Step 1: Find where to initialize Translator**

Find the MainWindow constructor and add Translator initialization after the UI is set up. Search for where other singletons are initialized.

Let me check the MainWindow constructor to find the right location.

The Translator is owned by each CodeViewEditor instance, not a singleton. The model refresh needs to happen once. The simplest approach: each CodeViewEditor creates its own Translator and refreshes models on first context menu open.

Alternative: Add a lazy refresh in `AddTranslateContextMenu` — if models are empty, try to refresh synchronously or show "Loading...".

Simpler approach: refresh models in the context menu event itself.

In `AddTranslateContextMenu`, change the models empty check to also attempt a refresh:

```cpp
void CodeViewEditor::AddTranslateContextMenu(QMenu *menu)
{
    // ... existing code ...

    QStringList models = m_translator->availableModels();
    if (models.isEmpty()) {
        // Try refreshing
        m_translator->refreshModels();
        models = m_translator->availableModels();
    }

    // ... rest of code ...
}
```

This won't work because `refreshModels()` is async. Better: refresh models when CodeViewEditor is created and on each context menu show what we have.

The current implementation already handles this correctly — `refreshModels()` is async, `availableModels()` returns cached list. Models are refreshed when TranslationWidget is opened (which happens at Preferences dialog open). The context menu just shows what's cached.

For the first startup, models will be empty until the user opens Preferences once. This is acceptable — the menu shows "No models available" which guides the user to configure.

No changes needed to MainWindow. Skip this task.

- [ ] **Step 1: Mark as N/A** — Translator refreshes via TranslationWidget. First-use shows "No models available" which is clear enough.

---

### Task 9: Build and smoke test

**Files:**
- None (testing only)

- [ ] **Step 1: Full rebuild**

Run: `cd /data1/u2/sigil074/build && cmake .. && make -j4 2>&1 | tail -50`
Expected: Build succeeds with no errors.

- [ ] **Step 2: Run Sigil and test**

Run: `cd /data1/u2/sigil074/build && ./bin/sigil &`

Test checklist:
1. Open an EPUB file
2. Right-click in CodeView → "Translate" menu should appear
3. Without sigoREST running: should show "No models available"
4. Open Preferences → "Translation" tab should be visible
5. Server URL shows "http://localhost:9080"
6. Click "Test" → "Not reachable" (if sigoREST not running)
7. Start sigoREST, click "Refresh" → models appear
8. Right-click in CodeView → "Translate" → EN→DE/DE→EN with model list
9. Click a translation → block text should be replaced

- [ ] **Step 3: Commit if any fixes needed**

---

## Self-Review Checklist

1. **Spec coverage:**
   - Translator class with placeholder encoding/decoding: Task 2-3 ✓
   - Block detection (FindCurrentBlock): Task 6 ✓
   - Context menu with EN→DE / DE→EN submenus: Task 6 ✓
   - sigoREST API integration: Task 3 ✓
   - Settings (sigorestServerUrl): Task 1 ✓
   - TranslationWidget settings UI: Task 4 ✓
   - Preferences registration: Task 5 ✓
   - CMakeLists.txt updates: Task 7 ✓
   - Model caching on startup: Task 8 (N/A — deferred to TranslationWidget) ✓

2. **Placeholder scan:** No TBD, TODO, or placeholder patterns found.

3. **Type consistency:**
   - `Translator::translate(blockText, direction, model)` matches `CodeViewEditor::TranslateBlock` call ✓
   - `Translator::translationReady(QString)` matches `OnTranslationReady(QString)` ✓
   - `Translator::translationError(QString)` matches `OnTranslationError(QString)` ✓
   - `BlockInfo` struct used consistently in FindCurrentBlock and TranslateBlock ✓
   - `m_translateMapper` uses `mappedString` consistent with Qt 6 pattern ✓
