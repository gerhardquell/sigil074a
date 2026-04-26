#pragma once
#ifndef PREVIEW_WIDGET_H
#define PREVIEW_WIDGET_H

#include <QWidget>
#include <QString>
#include <QUrl>
#include <QWebEngineView>
#include <QWebEnginePage>

class QTimer;

/**
 * A QWebEnginePage subclass that intercepts link clicks in the preview.
 * Regular navigation is blocked; link clicks are emitted as signals
 * so the application can handle them (e.g., open in CodeView).
 */
class PreviewPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit PreviewPage(QObject *parent = nullptr)
        : QWebEnginePage(parent) {}

signals:
    void linkClicked(const QUrl &url);

protected:
    bool acceptNavigationRequest(const QUrl &url, NavigationType type, bool isMainFrame) override
    {
        if (type == NavigationTypeLinkClicked) {
            emit linkClicked(url);
            return false;  // Block navigation — let the app handle it
        }
        return QWebEnginePage::acceptNavigationRequest(url, type, isMainFrame);
    }
};

class PreviewWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PreviewWidget(QWidget *parent = nullptr);
    ~PreviewWidget();

    void setHtml(const QString &html, const QUrl &baseUrl = QUrl());
    void scheduleUpdate(const QString &html, const QUrl &baseUrl = QUrl());
    void scrollToElement(const QString &elementId);
    void scrollToCaretElement(const QString &cssSelector);
    QWebEngineView *webView() const;

    void setupUrlHandler(const QString &basePath);

public slots:
    void updatePreview();

signals:
    void linkClicked(const QUrl &url);
    void loadFinished(bool ok);

private slots:
    void onLoadFinished(bool ok);

private:
    void setupUi();
    void connectSignals();

    QString convertUrlsToScheme(const QString &html, const QString &basePath) const;

    QWebEngineView *m_webView;
    QTimer *m_updateTimer;
    QString m_pendingHtml;
    QUrl m_pendingBaseUrl;
    bool m_isLoading;
    QString m_pendingCaretSelector;
    QString m_basePath;
};

#endif
