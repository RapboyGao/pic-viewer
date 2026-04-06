#pragma once

#include <QString>

namespace AppI18n {

enum class Language {
    English = 0,
    Chinese = 1,
};

Language currentLanguage();
void setCurrentLanguage(Language language);
QString uiText(const QString& english, const QString& chinese);
QString uiText(const char* english, const char* chinese);
QString text(const QString& english, const QString& chinese);
QString text(const char* english, const char* chinese);
QString languageDisplayName(Language language);

} // namespace AppI18n
