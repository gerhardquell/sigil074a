/************************************************************************
**
**  Copyright (C) 2026 Sigil Contributors
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

#include "PreviewWidget.h"

#include <QVBoxLayout>
#include <QTimer>
#include <QWebEnginePage>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QRegularExpression>
#include <QRegularExpressionMatchIterator>
#include <QFileInfo>
#include <QDir>
#include <QSize>

PreviewWidget::PreviewWidget(QWidget *parent)
    : QWidget(parent),
      m_webView(nullptr),
      m_updateTimer(nullptr),
      m_isLoading(false),
      m_pendingCaretSelector()
{
    setupUi();
    connectSignals();
}

PreviewWidget::~PreviewWidget()
{
}

void PreviewWidget::setupUi()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_webView = new QWebEngineView(this);
    m_webView->setContextMenuPolicy(Qt::NoContextMenu);

    // Use custom page that intercepts link clicks instead of navigating
    PreviewPage *page = new PreviewPage(m_webView);
    m_webView->setPage(page);

    // Critical: QWebEngineView default sizeHint is tiny (e.g. 200x30).
    // Without Expanding policy + minimum size, the splitter collapses it.
    m_webView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_webView->setMinimumSize(200, 200);

    // Configure settings for read-only preview
    QWebEngineSettings *settings = m_webView->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, false);
    settings->setAttribute(QWebEngineSettings::LocalContentCanAccessFileUrls, true);

    layout->addWidget(m_webView);
    setLayout(layout);

    // Setup debounce timer (400ms single shot)
    m_updateTimer = new QTimer(this);
    m_updateTimer->setSingleShot(true);
    m_updateTimer->setInterval(400);
}

void PreviewWidget::connectSignals()
{
    connect(m_updateTimer, &QTimer::timeout, this, &PreviewWidget::updatePreview);
    connect(m_webView, &QWebEngineView::loadFinished, this, &PreviewWidget::onLoadFinished);

    // Forward real link clicks from PreviewPage
    PreviewPage *page = qobject_cast<PreviewPage*>(m_webView->page());
    if (page) {
        connect(page, &PreviewPage::linkClicked, this, &PreviewWidget::linkClicked);
    }
}

void PreviewWidget::setHtml(const QString &html, const QUrl &baseUrl)
{
    m_pendingHtml = html;
    m_pendingBaseUrl = baseUrl;
    updatePreview();
}

void PreviewWidget::scheduleUpdate(const QString &html, const QUrl &baseUrl)
{
    m_pendingHtml = html;
    m_pendingBaseUrl = baseUrl;
    m_updateTimer->start();
}

void PreviewWidget::updatePreview()
{
    if (m_pendingHtml.isEmpty()) {
        return;
    }

    // Use file:// base URL from the HTML resource's filesystem path.
    // This lets the browser resolve relative URLs (../Styles/stylesheet.css)
    // natively against the actual file system, which Chromium handles reliably.
    // The sigil:// scheme is kept as a fallback for resources that can't be
    // served via file:// (e.g., if the basePath changes).
    QUrl baseUrl = m_pendingBaseUrl;
    if (baseUrl.scheme() != "file" && !m_basePath.isEmpty()) {
        // Convert filesystem path to file:// URL
        baseUrl = QUrl::fromLocalFile(m_basePath + "/dummy.xhtml");
    }

    m_isLoading = true;
    m_webView->setHtml(m_pendingHtml, baseUrl);
}

void PreviewWidget::scrollToElement(const QString &elementId)
{
    if (elementId.isEmpty()) {
        return;
    }

    QString js = QString(
        "var element = document.getElementById('%1');"
        "if (element) {"
        "    element.scrollIntoView({behavior: 'smooth', block: 'center'});"
        "}"
    ).arg(elementId);

    m_webView->page()->runJavaScript(js);
}

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

QWebEngineView *PreviewWidget::webView() const
{
    return m_webView;
}

void PreviewWidget::onLoadFinished(bool ok)
{
    m_isLoading = false;
    emit loadFinished(ok);

    if (ok && !m_pendingCaretSelector.isEmpty()) {
        scrollToCaretElement(m_pendingCaretSelector);
        m_pendingCaretSelector.clear();
    }
}

void PreviewWidget::setupUrlHandler(const QString &basePath)
{
    m_basePath = basePath;
}

QString PreviewWidget::convertUrlsToScheme(const QString &html, const QString &basePath) const
{
    if (basePath.isEmpty()) {
        return html;
    }

    QString result = html;
    QDir baseDir(basePath);

    // Regex patterns for different URL types in HTML attributes
    static const QRegularExpression hrefRegex(
        R"((href\s*=\s*["'])([^"']+)(["']))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression srcRegex(
        R"((src\s*=\s*["'])([^"']+)(["']))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression srcsetRegex(
        R"((srcset\s*=\s*["'])([^"']+)(["']))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression posterRegex(
        R"((poster\s*=\s*["'])([^"']+)(["']))",
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression cssUrlRegex(
        R"((url\s*\(\s*["']?)([^"')]+)(["']?\s*\)))",
        QRegularExpression::CaseInsensitiveOption);

    auto convertUrl = [&](const QString &url) -> QString {
        if (url.contains("://") || url.startsWith("//") || url.startsWith("#")) {
            return url;
        }
        if (url.startsWith("data:", Qt::CaseInsensitive) ||
            url.startsWith("javascript:", Qt::CaseInsensitive)) {
            return url;
        }
        QString absolutePath = baseDir.absoluteFilePath(url);
        QFileInfo fileInfo(absolutePath);
        if (fileInfo.exists()) {
            QString relativeToOebps = baseDir.relativeFilePath(absolutePath);
            return QString("sigil://book/%1").arg(relativeToOebps);
        }
        return url;
    };

    // Helper: collect matches, then apply replacements in reverse order
    // so that earlier replacements don't invalidate later positions.
    auto applyReplacementsReverse = [&](const QRegularExpression &regex, bool isSrcset = false) {
        QList<QRegularExpressionMatch> matches;
        QRegularExpressionMatchIterator it = regex.globalMatch(result);
        while (it.hasNext()) {
            matches.append(it.next());
        }

        int converted = 0;
        for (int i = matches.size() - 1; i >= 0; --i) {
            QRegularExpressionMatch &match = matches[i];
            if (isSrcset) {
                QString srcsetValue = match.captured(2);
                QStringList parts = srcsetValue.split(',');
                QStringList convertedParts;
                for (const QString &part : parts) {
                    QString trimmed = part.trimmed();
                    int spaceIdx = trimmed.indexOf(QRegularExpression("\\s"));
                    QString url = spaceIdx > 0 ? trimmed.left(spaceIdx) : trimmed;
                    QString descriptor = spaceIdx > 0 ? trimmed.mid(spaceIdx) : "";
                    convertedParts.append(convertUrl(url) + descriptor);
                }
                QString newSrcset = convertedParts.join(", ");
                result.replace(match.capturedStart(), match.capturedLength(),
                              match.captured(1) + newSrcset + match.captured(3));
                converted++;
            } else {
                QString originalUrl = match.captured(2);
                QString convertedUrl = convertUrl(originalUrl);
                if (originalUrl != convertedUrl) {
                    result.replace(match.capturedStart(), match.capturedLength(),
                                  match.captured(1) + convertedUrl + match.captured(3));
                    converted++;
                }
            }
        }
        return converted;
    };

    int total = 0;
    total += applyReplacementsReverse(hrefRegex);
    total += applyReplacementsReverse(srcRegex);
    total += applyReplacementsReverse(srcsetRegex, true);
    total += applyReplacementsReverse(posterRegex);
    total += applyReplacementsReverse(cssUrlRegex);

    return result;
}
