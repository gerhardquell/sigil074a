/************************************************************************
**
**  Copyright (C) 2012 John Schember <john@nachtimwald.com>
**  Copyright (C) 2012  Dave Heiland
**  Copyright (C) 2012  Grant Drake
**  Copyright (C) 2009, 2010, 2011  Strahinja Markovic  <strahinja.markovic@gmail.com>
**
**  This file is part of Sigil.
**
**  Sigil is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Sigil is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Sigil.  If not, see <http://www.gnu.org/licenses/>.
**
*************************************************************************/

#include <QtCore/QTimer>
#include <QtCore/QUrl>
#include <QApplication>
#include <QAction>
#include <QDialog>
#include <QLayout>
#include <QtPrintSupport/QPrinter>
#include <QtPrintSupport/QPrintDialog>
#include <QtPrintSupport/QPrintPreviewDialog>
#include <QSplitter>
#include <QFileInfo>
#include <QDir>
#include <QSet>

#include "BookManipulation/CleanSource.h"
#include "MiscEditors/ClipEditorModel.h"
#include "Misc/SettingsStore.h"
#include "Misc/Utility.h"
#include "ResourceObjects/HTMLResource.h"
#include "sigil_constants.h"
#include "Tabs/FlowTab.h"
#include "Tabs/WellFormedCheckComponent.h"
#include "ViewEditors/CodeViewEditor.h"
#include "ViewEditors/PreviewWidget.h"

static const QString SETTINGS_GROUP = "flowtab";

FlowTab::FlowTab(HTMLResource &resource,
                 const QUrl &fragment,
                 int view_state,
                 int line_to_scroll_to,
                 int position_to_scroll_to,
                 QString caret_location_to_scroll_to,
                 bool grab_focus,
                 QWidget *parent)
    :
    ContentTab(resource, parent),
    m_FragmentToScroll(fragment),
    m_LineToScrollTo(line_to_scroll_to),
    m_PositionToScrollTo(position_to_scroll_to),
    m_CaretLocationToScrollTo(caret_location_to_scroll_to),
    m_HTMLResource(resource),
    m_Splitter(new QSplitter(this)),
    m_CodeViewEditor(nullptr),
    m_PreviewWidget(nullptr),
    m_WellFormedCheckComponent(*new WellFormedCheckComponent(*this, parent)),
    m_initialLoad(true),
    m_grabFocus(grab_focus),
    m_cursorSyncTimer(nullptr)
{
    // Loading a flow tab can take a while. We set the wait
    // cursor and clear it at the end of the delayed initialization.
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Create the editors and set up the splitter
    CreateEditors();

    // Make sure the resource is loaded as its file doesn't seem
    // to exist when the resource tries to do an initial load.
    m_HTMLResource.InitialLoad();

    m_Layout.addWidget(m_Splitter);
    LoadSettings();

    // Set focus proxy to the code view editor
    setFocusProxy(m_CodeViewEditor);
    ConnectSignalsToSlots();

    // Cursor sync debounce timer (200ms)
    m_cursorSyncTimer = new QTimer(this);
    m_cursorSyncTimer->setSingleShot(true);
    m_cursorSyncTimer->setInterval(200);
    connect(m_cursorSyncTimer, &QTimer::timeout, this, &FlowTab::syncPreviewToCursor);

    // We perform delayed initialization after the widget is on
    // the screen. This way, the user perceives less load time.
    QTimer::singleShot(0, this, SLOT(DelayedInitialization()));
}

FlowTab::~FlowTab()
{
    // Stop cursor sync timer before disconnecting
    if (m_cursorSyncTimer) {
        m_cursorSyncTimer->stop();
    }
    // Disconnect CodeViewEditor signals before destroying to prevent
    // updatePreview being called during destruction
    if (m_CodeViewEditor) {
        disconnect(m_CodeViewEditor, nullptr, this, nullptr);
    }
    // Explicitly disconnect signals to prevent callbacks after deletion
    disconnect();
    m_WellFormedCheckComponent.deleteLater();

    // Widgets are children of the splitter, so they will be deleted automatically
    if (m_Splitter) {
        delete m_Splitter;
        m_Splitter = nullptr;
    }
}

void FlowTab::CreateEditors()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Create the CodeView editor
    m_CodeViewEditor = new CodeViewEditor(CodeViewEditor::Highlight_XHTML, true, this);
    m_CodeViewEditor->SetReformatHTMLEnabled(true);

    // Create the Preview widget
    m_PreviewWidget = new PreviewWidget(this);

    // Add both widgets to the splitter
    m_Splitter->addWidget(m_CodeViewEditor);
    m_Splitter->addWidget(m_PreviewWidget);

    // Set equal sizes for the splitter
    QList<int> sizes;
    sizes << 500 << 500;
    m_Splitter->setSizes(sizes);

    QApplication::restoreOverrideCursor();
}

void FlowTab::DelayedInitialization()
{
    // Setup URL handler FIRST, so that when the document is loaded
    // and triggers updatePreview(), the basePath is already set.
    QString htmlFilePath = m_HTMLResource.GetFullPath();
    QString oebpsPath = QFileInfo(htmlFilePath).absolutePath();
    QDir oebpsDir(oebpsPath);
    if (oebpsDir.dirName().toLower() == "text" || oebpsDir.dirName().toLower() == "html" || oebpsDir.dirName().toLower() == "xhtml") {
        oebpsDir.cdUp();
    }
    m_PreviewWidget->setupUrlHandler(oebpsDir.absolutePath());

    // Set up the CodeView with the document (this fires DocumentSet → updatePreview)
    m_CodeViewEditor->CustomSetDocument(m_HTMLResource.GetTextDocumentForWriting());
    // Zoom factor for CodeView can only be set when document has been loaded.
    m_CodeViewEditor->Zoom();

    // Scroll to the requested position
    if (m_PositionToScrollTo > 0) {
        m_CodeViewEditor->ScrollToPosition(m_PositionToScrollTo);
    } else if (m_LineToScrollTo > 0) {
        m_CodeViewEditor->ScrollToLine(m_LineToScrollTo);
    } else if (!m_FragmentToScroll.toString().isEmpty()) {
        m_CodeViewEditor->ScrollToFragment(m_FragmentToScroll.toString());
    }

    m_initialLoad = false;
    // Only now will we wire up monitoring of ResourceChanged, to prevent
    // unnecessary saving and marking of the resource for reloading.
    DelayedConnectSignalsToSlots();
    // Cursor set in constructor
    QApplication::restoreOverrideCursor();
}


bool FlowTab::IsLoadingFinished()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsLoadingFinished();
    }

    return true;
}

bool FlowTab::IsModified()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->document()->isModified();
    }

    return false;
}


// LoadTabContent is no longer needed in the split view model
// The CodeViewEditor is directly connected to the QTextDocument
void FlowTab::LoadTabContent()
{
}

void FlowTab::SaveTabContent()
{
    // In the split view model, the CodeViewEditor is directly connected to the QTextDocument
    // so the resource is already "saved". We just need to reset the modified state.
    m_HTMLResource.GetTextDocumentForWriting().setModified(false);
}

void FlowTab::ResourceModified()
{
    // This slot tells us that the underlying HTML resource has been changed
    // Schedule a preview update
    updatePreview();
}

void FlowTab::ResourceTextChanging()
{
    // Store the caret location so it can be restored after the resource is modified
    if (m_CodeViewEditor) {
        m_CodeViewEditor->StoreCaretLocationUpdate(m_CodeViewEditor->GetCaretLocation());
    }
}

void FlowTab::ReloadTabIfPending()
{
    if (!isVisible()) {
        return;
    }

    setFocus();

    // In the split view model, we just update the preview
    updatePreview();
}

void FlowTab::LeaveEditor(QWidget *editor)
{
    SaveTabContent();
}

void FlowTab::LoadSettings()
{
    UpdateDisplay();

    // SettingsChanged can fire for wanting the spelling highlighting to be refreshed on the tab.
    if (m_CodeViewEditor) {
        m_CodeViewEditor->RefreshSpellingHighlighting();
    }
}

void FlowTab::UpdateDisplay()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->UpdateDisplay();
    }
}

void FlowTab::EmitContentChanged()
{
    emit ContentChanged();
    // Schedule a preview update when content changes
    onCodeViewTextChanged();
}

void FlowTab::EmitUpdatePreview()
{
    emit UpdatePreview();
}

void FlowTab::EmitUpdatePreviewImmediately()
{
    emit UpdatePreviewImmediately();
}

void FlowTab::EmitUpdateCursorPosition()
{
    emit UpdateCursorPosition(GetCursorLine(), GetCursorColumn());
}

void FlowTab::onCodeViewTextChanged()
{
    // Schedule a debounced preview update
    if (m_PreviewWidget && m_CodeViewEditor) {
        QString html = m_CodeViewEditor->toPlainText();
        m_PreviewWidget->scheduleUpdate(html, QUrl::fromLocalFile(m_HTMLResource.GetFullPath()));
    }
}

void FlowTab::onCursorPositionChanged()
{
    EmitUpdateCursorPosition();
    if (m_cursorSyncTimer) {
        m_cursorSyncTimer->start();
    }
}

void FlowTab::syncPreviewToCursor()
{
    if (!m_CodeViewEditor || !m_PreviewWidget || !m_PreviewWidget->isVisible() || m_PreviewWidget->width() < 10) {
        return;
    }

    QList< ViewEditor::ElementIndex > hierarchy = m_CodeViewEditor->GetCaretLocation();
    QString selector = ElementIndexToCssSelector(hierarchy);

    if (selector.isEmpty()) {
        return;
    }

    m_PreviewWidget->scrollToCaretElement(selector);
}

QString FlowTab::ElementIndexToCssSelector(const QList< ViewEditor::ElementIndex > &hierarchy) const
{
    static const QSet<QString> blockElements = QSet<QString>()
        << "p" << "div" << "h1" << "h2" << "h3" << "h4" << "h5" << "h6"
        << "blockquote" << "ul" << "ol" << "li" << "table" << "section"
        << "article" << "header" << "footer" << "aside" << "nav"
        << "figure" << "figcaption" << "dl" << "pre" << "hr" << "address"
        << "main" << "details" << "summary";

    auto sanitizeName = [](const QString &name) -> QString {
        int colonPos = name.indexOf(':');
        return colonPos >= 0 ? name.mid(colonPos + 1) : name;
    };

    // Skip html, body — too high-level for useful scrolling
    // Walk from leaf to root, find deepest block-level element
    int blockIndex = -1;
    for (int i = hierarchy.size() - 1; i >= 0; --i) {
        if (blockElements.contains(sanitizeName(hierarchy[i].name))) {
            blockIndex = i;
            break;
        }
    }

    if (blockIndex < 0) {
        return QString();
    }

    const ViewEditor::ElementIndex &block = hierarchy[blockIndex];
    QString blockName = sanitizeName(block.name);
    // ElementIndex.index is 0-based child position among all siblings, :nth-child is 1-based
    QString selector = QString("%1:nth-child(%2)")
                           .arg(blockName)
                           .arg(block.index + 1);

    // If there's an inline child after the block, append it for precision
    if (blockIndex + 1 < hierarchy.size()) {
        const ViewEditor::ElementIndex &child = hierarchy[blockIndex + 1];
        QString childName = sanitizeName(child.name);
        selector += QString(" > %1:nth-child(%2)")
                        .arg(childName)
                        .arg(child.index + 1);
    }

    return selector;
}

void FlowTab::updatePreview()
{
    // Update the preview immediately
    if (m_PreviewWidget && m_CodeViewEditor) {
        QString html = m_CodeViewEditor->toPlainText();
        m_PreviewWidget->setHtml(html, QUrl::fromLocalFile(m_HTMLResource.GetFullPath()));
    }
}

void FlowTab::HighlightWord(QString word, int pos)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->HighlightWord(word, pos);
    }
}

void FlowTab::RefreshSpellingHighlighting()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->RefreshSpellingHighlighting();
    }
}


bool FlowTab::CutEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->textCursor().hasSelection();
    }

    return false;
}

bool FlowTab::CopyEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->textCursor().hasSelection();
    }

    return false;
}

bool FlowTab::PasteEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->canPaste();
    }

    return false;
}

bool FlowTab::DeleteLineEnabled()
{
    if (m_CodeViewEditor) {
        return !m_CodeViewEditor->document()->isEmpty();
    }

    return false;
}

bool FlowTab::RemoveFormattingEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsCutCodeTagsAllowed();
    }

    return false;
}

bool FlowTab::InsertClosingTagEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsInsertClosingTagAllowed();
    }

    return false;
}

bool FlowTab::AddToIndexEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsAddToIndexAllowed();
    }

    return false;
}

bool FlowTab::MarkForIndexEnabled()
{
    if (m_CodeViewEditor) {
        return true;
    }

    return false;
}

bool FlowTab::InsertIdEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsInsertIdAllowed();
    }

    return false;
}

bool FlowTab::InsertHyperlinkEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsInsertHyperlinkAllowed();
    }

    return false;
}

bool FlowTab::InsertSpecialCharacterEnabled()
{
    return m_CodeViewEditor != nullptr;
}

bool FlowTab::InsertFileEnabled()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->IsInsertFileAllowed();
    }

    return false;
}

bool FlowTab::ToggleAutoSpellcheckEnabled()
{
    return m_CodeViewEditor != nullptr;
}

void FlowTab::GoToCaretLocation(QList< ViewEditor::ElementIndex > location)
{
    if (location.isEmpty() || !m_CodeViewEditor) {
        return;
    }
    m_CodeViewEditor->StoreCaretLocationUpdate(location);
    m_CodeViewEditor->ExecuteCaretUpdate();
}

QList< ViewEditor::ElementIndex > FlowTab::GetCaretLocation()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->GetCaretLocation();
    }

    return QList<ViewEditor::ElementIndex>();
}

QString FlowTab::GetCaretLocationUpdate() const
{
    return QString();
}

QString FlowTab::GetDisplayedCharacters()
{
    return "";
}

QString FlowTab::GetText()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->toPlainText();
    }

    return "";
}

int FlowTab::GetCursorPosition() const
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->GetCursorPosition();
    }

    return -1;
}

int FlowTab::GetCursorLine() const
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->GetCursorLine();
    }

    return -1;
}

int FlowTab::GetCursorColumn() const
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->GetCursorColumn();
    }

    return -1;
}

float FlowTab::GetZoomFactor() const
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->GetZoomFactor();
    }

    return 1;
}

void FlowTab::SetZoomFactor(float new_zoom_factor)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->SetZoomFactor(new_zoom_factor);
    }
}

Searchable *FlowTab::GetSearchableContent()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor;
    }

    return nullptr;
}


void FlowTab::ScrollToFragment(const QString &fragment)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ScrollToFragment(fragment);
    }
    if (m_PreviewWidget) {
        m_PreviewWidget->scrollToElement(fragment);
    }
}

void FlowTab::ScrollToLine(int line)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ScrollToLine(line);
    }
}

void FlowTab::ScrollToPosition(int cursor_position)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ScrollToPosition(cursor_position);
    }
}

void FlowTab::ScrollToCaretLocation(QString caret_location_update)
{
    // Not applicable in split view model
    Q_UNUSED(caret_location_update)
}

void FlowTab::ScrollToTop()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ScrollToTop();
    }
    if (m_PreviewWidget) {
        // Scroll preview to top via JavaScript
        m_PreviewWidget->webView()->page()->runJavaScript("window.scrollTo(0, 0);");
    }
}

void FlowTab::AutoFixWellFormedErrors()
{
    if (m_CodeViewEditor) {
        int pos = m_CodeViewEditor->GetCursorPosition();
        m_CodeViewEditor->ReplaceDocumentText(CleanSource::ToValidXHTML(m_CodeViewEditor->toPlainText()));
        m_CodeViewEditor->ScrollToPosition(pos);
    }
}

void FlowTab::TakeControlOfUI()
{
    EmitCentralTabRequest();
    setFocus();
}

QString FlowTab::GetFilename()
{
    return ContentTab::GetFilename();
}

bool FlowTab::IsDataWellFormed()
{
    // In the split view model, we only have CodeView
    // Check if the HTML is well formed
    XhtmlDoc::WellFormedError error = (m_CodeViewEditor != nullptr)
                                          ? XhtmlDoc::WellFormedErrorForSource(m_CodeViewEditor->toPlainText())
                                          : XhtmlDoc::WellFormedErrorForSource(m_HTMLResource.GetText());
    bool wellFormed = error.line == -1;

    if (!wellFormed) {
        m_WellFormedCheckComponent.DemandAttentionIfAllowed(error);
    }

    return wellFormed;
}

void FlowTab::Undo()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->undo();
    }
}

void FlowTab::Redo()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->redo();
    }
}

void FlowTab::Cut()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->cut();
    }
}

void FlowTab::Copy()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->copy();
    }
}

void FlowTab::Paste()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->paste();
    }
}

void FlowTab::DeleteLine()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->DeleteLine();
    }
}

bool FlowTab::MarkSelection()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->MarkSelection();
    }
    return false;
}

bool FlowTab::ClearMarkedText()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->ClearMarkedText();
    }
    return false;
}

void FlowTab::SplitSection()
{
    if (!IsDataWellFormed()) {
        return;
    }

    if (m_CodeViewEditor) {
        emit OldTabRequest(m_CodeViewEditor->SplitSection(), m_HTMLResource);
    }
}

void FlowTab::InsertSGFSectionMarker()
{
    if (!IsDataWellFormed()) {
        return;
    }

    if (m_CodeViewEditor) {
        m_CodeViewEditor->InsertSGFSectionMarker();
    }
}

void FlowTab::InsertClosingTag()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->InsertClosingTag();
    }
}

void FlowTab::AddToIndex()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->AddToIndex();
    }
}

bool FlowTab::MarkForIndex(const QString &title)
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->MarkForIndex(title);
    }

    return false;
}

QString FlowTab::GetAttributeId()
{
    QString attribute_value;

    if (m_CodeViewEditor) {
        // We are only interested in ids on <a> anchor elements
        attribute_value = m_CodeViewEditor->GetAttributeId();
    }

    return attribute_value;
}

QString FlowTab::GetAttributeHref()
{
    QString attribute_value;

    if (m_CodeViewEditor) {
        attribute_value = m_CodeViewEditor->GetAttribute("href", ANCHOR_TAGS, false, true);
    }

    return attribute_value;
}

QString FlowTab::GetAttributeIndexTitle()
{
    QString attribute_value;

    if (m_CodeViewEditor) {
        attribute_value = m_CodeViewEditor->GetAttribute("title", ANCHOR_TAGS, false, true);

        if (attribute_value.isEmpty()) {
            attribute_value = m_CodeViewEditor->GetSelectedText();
        }
    }

    return attribute_value;
}

QString FlowTab::GetSelectedText()
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->GetSelectedText();
    }

    return "";
}

bool FlowTab::InsertId(const QString &id)
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->InsertId(id);
    }

    return false;
}

bool FlowTab::InsertHyperlink(const QString &href)
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->InsertHyperlink(href);
    }

    return false;
}

void FlowTab::InsertFile(QString html)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->insertPlainText(html);
    }
}

void FlowTab::PrintPreview()
{
    QPrintPreviewDialog *print_preview = new QPrintPreviewDialog(this);

    if (m_CodeViewEditor) {
        connect(print_preview, SIGNAL(paintRequested(QPrinter *)), m_CodeViewEditor, SLOT(print(QPrinter *)));
    } else {
        return;
    }

    print_preview->exec();
    print_preview->deleteLater();
}

void FlowTab::Print()
{
    QPrinter printer;
    QPrintDialog print_dialog(&printer, this);
    print_dialog.setWindowTitle(tr("Print %1").arg(GetFilename()));

    if (print_dialog.exec() == QDialog::Accepted) {
        if (m_CodeViewEditor) {
            m_CodeViewEditor->print(&printer);
        }
    }
}

void FlowTab::Bold()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ToggleFormatSelection("b", "font-weight", "bold");
    }
}

void FlowTab::Italic()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ToggleFormatSelection("i", "font-style", "italic");
    }
}

void FlowTab::Underline()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ToggleFormatSelection("u", "text-decoration", "underline");
    }
}

void FlowTab::Strikethrough()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ToggleFormatSelection("strike", "text-decoration", "line-through");
    }
}

void FlowTab::Subscript()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ToggleFormatSelection("sub");
    }
}

void FlowTab::Superscript()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ToggleFormatSelection("sup");
    }
}

void FlowTab::AlignLeft()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("text-align", "left");
    }
}

void FlowTab::AlignCenter()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("text-align", "center");
    }
}

void FlowTab::AlignRight()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("text-align", "right");
    }
}

void FlowTab::AlignJustify()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("text-align", "justify");
    }
}

void FlowTab::InsertBulletedList()
{
    // Not supported in CodeView
}

void FlowTab::InsertNumberedList()
{
    // Not supported in CodeView
}

void FlowTab::DecreaseIndent()
{
    // Not supported in CodeView
}

void FlowTab::IncreaseIndent()
{
    // Not supported in CodeView
}

void FlowTab::TextDirectionLeftToRight()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("direction", "ltr");
    }
}

void FlowTab::TextDirectionRightToLeft()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("direction", "rtl");
    }
}

void FlowTab::TextDirectionDefault()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->FormatStyle("direction", "inherit");
    }
}

void FlowTab::ShowTag()
{
    // Not supported in CodeView
}

void FlowTab::RemoveFormatting()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->CutCodeTags();
    }
}

void FlowTab::ChangeCasing(const Utility::Casing casing)
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->ApplyCaseChangeToSelection(casing);
    }
}

void FlowTab::HeadingStyle(const QString &heading_type, bool preserve_attributes)
{
    if (m_CodeViewEditor) {
        QChar last_char = heading_type[ heading_type.count() - 1 ];

        // For heading_type == "Heading #"
        if (last_char.isDigit()) {
            m_CodeViewEditor->FormatBlock("h" % QString(last_char), preserve_attributes);
        } else if (heading_type == "Normal") {
            m_CodeViewEditor->FormatBlock("p", preserve_attributes);
        }
    }
}

void FlowTab::GoToLinkOrStyle()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->GoToLinkOrStyle();
    }
}

void FlowTab::AddMisspelledWord()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->AddMisspelledWord();
    }
}

void FlowTab::IgnoreMisspelledWord()
{
    if (m_CodeViewEditor) {
        m_CodeViewEditor->IgnoreMisspelledWord();
    }
}

bool FlowTab::BoldChecked()
{
    return ContentTab::BoldChecked();
}

bool FlowTab::ItalicChecked()
{
    return ContentTab::ItalicChecked();
}

bool FlowTab::UnderlineChecked()
{
    return ContentTab::UnderlineChecked();
}

bool FlowTab::StrikethroughChecked()
{
    return ContentTab::StrikethroughChecked();
}

bool FlowTab::SubscriptChecked()
{
    return ContentTab::SubscriptChecked();
}

bool FlowTab::SuperscriptChecked()
{
    return ContentTab::SuperscriptChecked();
}

bool FlowTab::AlignLeftChecked()
{
    return ContentTab::AlignLeftChecked();
}

bool FlowTab::AlignRightChecked()
{
    return ContentTab::AlignRightChecked();
}

bool FlowTab::AlignCenterChecked()
{
    return ContentTab::AlignCenterChecked();
}

bool FlowTab::AlignJustifyChecked()
{
    return ContentTab::AlignJustifyChecked();
}

bool FlowTab::BulletListChecked()
{
    return ContentTab::BulletListChecked();
}

bool FlowTab::NumberListChecked()
{
    return ContentTab::NumberListChecked();
}

bool FlowTab::PasteClipNumber(int clip_number)
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->PasteClipNumber(clip_number);
    }
    return false;
}

bool FlowTab::PasteClipEntries(QList< ClipEditorModel::clipEntry * > clips)
{
    if (m_CodeViewEditor) {
        return m_CodeViewEditor->PasteClipEntries(clips);
    }
    return false;
}

QString FlowTab::GetCaretElementName()
{
    return ContentTab::GetCaretElementName();
}

void FlowTab::SuspendTabReloading()
{
    // In the split view model, this is a no-op
}

void FlowTab::ResumeTabReloading()
{
    // In the split view model, this is a no-op
    // Just update the preview if needed
    updatePreview();
}

void FlowTab::DelayedConnectSignalsToSlots()
{
    connect(&m_HTMLResource, SIGNAL(TextChanging()), this, SLOT(ResourceTextChanging()));
    connect(&m_HTMLResource, SIGNAL(Modified()), this, SLOT(ResourceModified()));
    connect(&m_HTMLResource, SIGNAL(LoadedFromDisk()), this, SLOT(ReloadTabIfPending()));
}

void FlowTab::ConnectSignalsToSlots()
{
    // Connect CodeViewEditor signals
    connect(m_CodeViewEditor, SIGNAL(cursorPositionChanged()), this, SLOT(onCursorPositionChanged()));
    connect(m_CodeViewEditor, SIGNAL(ZoomFactorChanged(float)), this, SIGNAL(ZoomFactorChanged(float)));
    connect(m_CodeViewEditor, SIGNAL(selectionChanged()), this, SIGNAL(SelectionChanged()));
    connect(m_CodeViewEditor, SIGNAL(FocusLost(QWidget *)), this, SLOT(LeaveEditor(QWidget *)));
    connect(m_CodeViewEditor, SIGNAL(LinkClicked(const QUrl &)), this, SIGNAL(LinkClicked(const QUrl &)));
    connect(m_CodeViewEditor, SIGNAL(ViewImage(const QUrl &)), this, SIGNAL(ViewImageRequest(const QUrl &)));
    connect(m_CodeViewEditor, SIGNAL(OpenClipEditorRequest(ClipEditorModel::clipEntry *)), this, SIGNAL(OpenClipEditorRequest(ClipEditorModel::clipEntry *)));
    connect(m_CodeViewEditor, SIGNAL(OpenIndexEditorRequest(IndexEditorModel::indexEntry *)), this, SIGNAL(OpenIndexEditorRequest(IndexEditorModel::indexEntry *)));
    connect(m_CodeViewEditor, SIGNAL(GoToLinkedStyleDefinitionRequest(const QString &, const QString &)), this, SIGNAL(GoToLinkedStyleDefinitionRequest(const QString &, const QString &)));
    connect(m_CodeViewEditor, SIGNAL(BookmarkLinkOrStyleLocationRequest()), this, SIGNAL(BookmarkLinkOrStyleLocationRequest()));
    connect(m_CodeViewEditor, SIGNAL(SpellingHighlightRefreshRequest()), this, SIGNAL(SpellingHighlightRefreshRequest()));
    connect(m_CodeViewEditor, SIGNAL(ShowStatusMessageRequest(const QString &)), this, SIGNAL(ShowStatusMessageRequest(const QString &)));

    // All content changes → debounced preview update (via scheduleUpdate)
    connect(m_CodeViewEditor, SIGNAL(FilteredTextChanged()), this, SLOT(onCodeViewTextChanged()));
    connect(m_CodeViewEditor, SIGNAL(PageUpdated()), this, SLOT(onCodeViewTextChanged()));
    connect(m_CodeViewEditor, SIGNAL(DocumentSet()), this, SLOT(onCodeViewTextChanged()));

    connect(m_CodeViewEditor, SIGNAL(MarkSelectionRequest()), this, SIGNAL(MarkSelectionRequest()));
    connect(m_CodeViewEditor, SIGNAL(ClearMarkedTextRequest()), this, SIGNAL(ClearMarkedTextRequest()));

    // Connect PreviewWidget signals
    connect(m_PreviewWidget, SIGNAL(linkClicked(const QUrl &)), this, SIGNAL(LinkClicked(const QUrl &)));
}
