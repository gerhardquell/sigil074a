#include "Translator.h"
#include "SettingsStore.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QRegularExpressionMatch>
#include <QRegularExpressionMatchIterator>

const QStringList Translator::BLOCK_TAGS = QStringList()
    << "p" << "div" << "h1" << "h2" << "h3" << "h4" << "h5" << "h6"
    << "ul" << "ol" << "li" << "blockquote" << "pre" << "table"
    << "thead" << "tbody" << "tr" << "td" << "th";

const QStringList Translator::INLINE_TAGS = QStringList()
    << "em" << "strong" << "b" << "i" << "u" << "a" << "span"
    << "sub" << "sup" << "code" << "small" << "mark" << "abbr" << "cite" << "q";

Translator::Translator(QObject *parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
    , m_modelsReply(nullptr)
    , m_translateReply(nullptr)
    , m_serverUrl(SettingsStore::sigorestServerUrl())
{
}

Translator::~Translator()
{
    if (m_modelsReply && m_modelsReply->isRunning()) {
        m_modelsReply->abort();
    }
    if (m_translateReply && m_translateReply->isRunning()) {
        m_translateReply->abort();
    }
}

void Translator::translate(const QString &blockText, const QString &direction, const QString &model)
{
    if (m_serverUrl.isEmpty()) {
        emit translationError("Server URL not configured");
        return;
    }

    // Abort any ongoing translation
    if (m_translateReply && m_translateReply->isRunning()) {
        m_translateReply->abort();
        m_translateReply->deleteLater();
    }

    // Encode inline HTML tags as placeholders
    QString encodedText = encodePlaceholders(blockText);

    // Build JSON request
    QJsonObject requestJson;
    requestJson["model"] = model;

    QJsonArray messages;
    messages.append(QJsonObject{
        {"role", "system"},
        {"content", buildSystemPrompt(direction)}
    });
    messages.append(QJsonObject{
        {"role", "user"},
        {"content", buildUserPrompt(encodedText)}
    });
    requestJson["messages"] = messages;
    requestJson["max_tokens"] = 2000;
    requestJson["temperature"] = 0.3;

    QJsonDocument doc(requestJson);
    QByteArray jsonData = doc.toJson();

    QNetworkRequest request = createRequest("/v1/chat/completions");
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    m_translateReply = m_networkManager->post(request, jsonData);
    connect(m_translateReply, &QNetworkReply::finished, this, &Translator::onTranslateReplyFinished);
}

QStringList Translator::availableModels() const
{
    return m_cachedModels;
}

void Translator::refreshModels()
{
    if (m_serverUrl.isEmpty()) {
        emit modelsRefreshed();
        return;
    }

    // Abort any ongoing models request
    if (m_modelsReply && m_modelsReply->isRunning()) {
        m_modelsReply->abort();
        m_modelsReply->deleteLater();
    }

    QNetworkRequest request = createRequest("/v1/models");
    m_modelsReply = m_networkManager->get(request);
    connect(m_modelsReply, &QNetworkReply::finished, this, &Translator::onModelsReplyFinished);
}

bool Translator::isServerAvailable() const
{
    return !m_serverUrl.isEmpty() && !m_cachedModels.isEmpty();
}

void Translator::onModelsReplyFinished()
{
    if (!m_modelsReply) {
        emit modelsRefreshed();
        return;
    }

    QNetworkReply *reply = m_modelsReply;
    m_modelsReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        m_cachedModels.clear();
        emit modelsRefreshed();
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);

    if (!doc.isObject()) {
        m_cachedModels.clear();
        emit modelsRefreshed();
        reply->deleteLater();
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("data") || !root["data"].isArray()) {
        m_cachedModels.clear();
        emit modelsRefreshed();
        reply->deleteLater();
        return;
    }

    QJsonArray modelsArray = root["data"].toArray();
    m_cachedModels.clear();

    for (const QJsonValue &value : modelsArray) {
        if (value.isObject()) {
            QJsonObject modelObj = value.toObject();
            if (modelObj.contains("id") && modelObj["id"].isString()) {
                m_cachedModels.append(modelObj["id"].toString());
            }
        }
    }

    // Sort models alphabetically
    m_cachedModels.sort();

    emit modelsRefreshed();

    reply->deleteLater();
}

void Translator::onTranslateReplyFinished()
{
    if (!m_translateReply) {
        return;
    }

    QNetworkReply *reply = m_translateReply;
    m_translateReply = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        QString errorMsg = reply->errorString();
        emit translationError(errorMsg);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(responseData);

    if (!doc.isObject()) {
        emit translationError("Invalid response format");
        reply->deleteLater();
        return;
    }

    QJsonObject root = doc.object();
    if (!root.contains("choices") || !root["choices"].isArray()) {
        emit translationError("No choices in response");
        reply->deleteLater();
        return;
    }

    QJsonArray choices = root["choices"].toArray();
    if (choices.isEmpty()) {
        emit translationError("Empty choices array");
        reply->deleteLater();
        return;
    }

    QJsonObject choice = choices[0].toObject();
    if (!choice.contains("message") || !choice["message"].isObject()) {
        emit translationError("No message in choice");
        reply->deleteLater();
        return;
    }

    QJsonObject message = choice["message"].toObject();
    if (!message.contains("content") || !message["content"].isString()) {
        emit translationError("No content in message");
        reply->deleteLater();
        return;
    }

    QString translatedText = message["content"].toString();

    // Decode placeholders back to HTML tags
    QString decodedText = decodePlaceholders(translatedText);

    emit translationReady(decodedText);

    reply->deleteLater();
}

QString Translator::encodePlaceholders(const QString &html)
{
    m_savedTagAttributes.clear();
    QString result = html;
    int placeholderIndex = 0;

    // Process inline tags with attributes
    for (const QString &tag : INLINE_TAGS) {
        // Pattern to match tags with or without attributes
        QRegularExpression openingTagPattern(
            QString("<%1([^>]*)>").arg(QRegularExpression::escape(tag)),
            QRegularExpression::CaseInsensitiveOption
        );
        QRegularExpression closingTagPattern(
            QString("</%1>").arg(QRegularExpression::escape(tag)),
            QRegularExpression::CaseInsensitiveOption
        );

        int pos = 0;
        while (true) {
            QRegularExpressionMatch openingMatch = openingTagPattern.match(result, pos);
            if (!openingMatch.hasMatch()) {
                break;
            }

            int openingStart = openingMatch.capturedStart();
            QString attributes = openingMatch.captured(1).trimmed();

            // Find matching closing tag
            int searchStart = openingMatch.capturedEnd();
            QRegularExpressionMatch closingMatch = closingTagPattern.match(result, searchStart);
            if (!closingMatch.hasMatch()) {
                pos = searchStart;
                continue;
            }

            int closingStart = closingMatch.capturedStart();
            int closingEnd = closingMatch.capturedEnd();

            // Extract content between tags
            QString content = result.mid(openingMatch.capturedEnd(), closingStart - openingMatch.capturedEnd());

            // Save attributes if present
            QString key = QString("%1_%2").arg(placeholderIndex).arg(tag.toUpper());
            if (!attributes.isEmpty()) {
                m_savedTagAttributes[key] = attributes;
            }

            // Replace with placeholder
            QString placeholder = QString("%1_%2%3%4%5")
                .arg(placeholderIndex)
                .arg(tag.toUpper())
                .arg("\342\210\247")  // ⟦
                .arg(content)
                .arg("\342\210\247");  // ⟧

            result.replace(openingStart, closingEnd - openingStart, placeholder);
            placeholderIndex++;

            pos = openingStart + placeholder.length();
        }
    }

    // Remove remaining non-inline tags (block tags)
    for (const QString &tag : BLOCK_TAGS) {
        QRegularExpression tagPattern(
            QString("</?%1[^>]*>").arg(QRegularExpression::escape(tag)),
            QRegularExpression::CaseInsensitiveOption
        );
        result.remove(tagPattern);
    }

    return result;
}

QString Translator::decodePlaceholders(const QString &text)
{
    QString result = text;

    // Pattern to match placeholders: 0_TAG⟦content⟧TAG
    QRegularExpression placeholderPattern(
        "(\\d+)_(EM|STRONG|B|I|U|A|SPAN|SUB|SUP|CODE|SMALL|MARK|ABBR|CITE|Q)"
        "\342\210\247"  // ⟦
        "([^\\342\210\247]*)"  // content (anything except ⟦)
        "\342\210\247"  // ⟧
        "\\1_\\2",
        QRegularExpression::CaseInsensitiveOption
    );

    QRegularExpressionMatchIterator it = placeholderPattern.globalMatch(result);
    QStringList replacements;
    QList<QRegularExpressionMatch> matches;

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        matches.append(match);
    }

    // Process in reverse order to maintain positions
    for (int i = matches.size() - 1; i >= 0; i--) {
        QRegularExpressionMatch match = matches[i];
        int index = match.captured(1).toInt();
        QString tagName = match.captured(2).toLower();
        QString content = match.captured(3);
        QString key = QString("%1_%2").arg(index).arg(tagName.toUpper());

        // Escape HTML in content
        QString escapedContent = escapeHtml(content);

        // Build tag with attributes if saved
        QString fullTag;
        if (m_savedTagAttributes.contains(key)) {
            fullTag = QString("<%1 %2>%3</%1>").arg(tagName, m_savedTagAttributes[key], escapedContent);
        } else {
            fullTag = QString("<%1>%2</%1>").arg(tagName, escapedContent);
        }

        result.replace(match.capturedStart(), match.capturedLength(), fullTag);
    }

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
    // Extract target language from direction (e.g., "en→de" -> "German")
    QString targetLanguage = "the target language";

    if (direction.contains("\342\206\222")) {  // →
        QStringList parts = direction.split("\342\206\222");
        if (parts.size() == 2) {
            QString lang = parts[1].toLower().trimmed();
            if (lang == "en") targetLanguage = "English";
            else if (lang == "de") targetLanguage = "German";
            else if (lang == "fr") targetLanguage = "French";
            else if (lang == "es") targetLanguage = "Spanish";
            else if (lang == "it") targetLanguage = "Italian";
            else if (lang == "pt") targetLanguage = "Portuguese";
            else if (lang == "ru") targetLanguage = "Russian";
            else if (lang == "zh") targetLanguage = "Chinese";
            else if (lang == "ja") targetLanguage = "Japanese";
            else targetLanguage = lang;
        }
    }

    return QString("You are a translation engine. Translate the following text to %1. "
                    "Output only the translated text, no explanations. "
                    "Preserve all numeric placeholders like 0_EM and the markers \342\210\247 and \342\210\247 "
                    "exactly as they appear — do not translate, remove, or modify them.")
        .arg(targetLanguage);
}

QString Translator::buildUserPrompt(const QString &text)
{
    return text;
}

QNetworkRequest Translator::createRequest(const QString &endpoint)
{
    QUrl url(m_serverUrl + endpoint);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Sigil/0.7.4");
    return request;
}
