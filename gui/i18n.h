#pragma once
// The app's languages: English, Spanish and Catalan.
//
// Every text the user sees goes through T(), with the English text as the key:
//     primaryButton(T("Start combat"))
//     addLog(TF("%s takes %d", name.c_str(), amount), COL_RED)
// The translations live in one table in i18n.cpp (English, Spanish, Catalan per row).
// A text with no translation shows in English. Things that are saved (condition
// names, creature names) stay in English in the files and are translated only when
// shown, so a save opens the same in any language.
#include <set>
#include <string>

enum class Lang { English, Spanish, Catalan };
inline constexpr int LANG_COUNT = 3;

Lang language();
void setLanguage(Lang lang);
const char* langCode(Lang lang);                  // "en", "es", "ca"
Lang langFromCode(const std::string& code);       // English if unknown
// The first of the system's preferred languages that the app has (English if none).
Lang systemLanguage();

// `english` in the current language.
const char* T(const char* english);
const char* T(const std::string& english);
// For texts that may come from the user (condition names): translated when the table
// knows them, otherwise returned as they are (and not reported as missing).
const char* TKnown(const std::string& text);
// printf on the translated format. Translations keep the order of the %s / %d.
std::string TF(const char* englishFormat, ...);

// Keys asked for in Spanish or Catalan that have no translation (checked by the tests).
const std::set<std::string>& missingTranslations();
