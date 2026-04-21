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
