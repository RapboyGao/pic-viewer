#include "app/app_language.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QVariant>

namespace AppI18n {

namespace {

constexpr char kLanguageSettingKey[] = "ui/language";

Language defaultLanguage()
{
    const QString localeName = QLocale::system().name();
    return localeName.startsWith("zh", Qt::CaseInsensitive) ? Language::Chinese : Language::English;
}

Language& cachedLanguage()
{
    static Language language = []() {
        QSettings settings;
        const QVariant value = settings.value(kLanguageSettingKey);
        bool ok = false;
        const int rawLanguage = value.toInt(&ok);
        if (ok && (rawLanguage == static_cast<int>(Language::English) || rawLanguage == static_cast<int>(Language::Chinese))) {
            return static_cast<Language>(rawLanguage);
        }
        return defaultLanguage();
    }();
    return language;
}

} // namespace

Language currentLanguage()
{
    return cachedLanguage();
}

void setCurrentLanguage(Language language)
{
    cachedLanguage() = language;
    QSettings settings;
    settings.setValue(kLanguageSettingKey, static_cast<int>(language));
    settings.sync();
}

QString uiText(const QString& english, const QString& chinese)
{
    return currentLanguage() == Language::Chinese ? chinese : english;
}

QString uiText(const char* english, const char* chinese)
{
    return uiText(QString::fromUtf8(english), QString::fromUtf8(chinese));
}

QString text(const QString& english, const QString& chinese)
{
    return uiText(english, chinese);
}

QString text(const char* english, const char* chinese)
{
    return uiText(english, chinese);
}

QString languageDisplayName(Language language)
{
    return language == Language::Chinese ? QStringLiteral("中文") : QStringLiteral("English");
}

} // namespace AppI18n
