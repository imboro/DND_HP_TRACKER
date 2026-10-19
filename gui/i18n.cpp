#include "i18n.h"

#include <SDL.h>

#include <cstdarg>
#include <cstdio>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

struct Row {
    const char* en;
    const char* es;
    const char* ca;
};

// The translation table: one row per text, as TRANSLATIONS[] (English, Spanish, Catalan).
#include "i18n_table.inc"

Lang g_lang = Lang::English;
std::set<std::string> g_missing;

// English text -> row, built on first use.
const std::unordered_map<std::string_view, const Row*>& rows() {
    static const std::unordered_map<std::string_view, const Row*> map = [] {
        std::unordered_map<std::string_view, const Row*> m;
        for (const Row& r : TRANSLATIONS) m.emplace(r.en, &r);
        return m;
    }();
    return map;
}

}  // namespace

Lang language() { return g_lang; }

void setLanguage(Lang lang) { g_lang = lang; }

const char* langCode(Lang lang) {
    switch (lang) {
        case Lang::Spanish: return "es";
        case Lang::Catalan: return "ca";
        default: return "en";
    }
}

Lang langFromCode(const std::string& code) {
    if (code.rfind("es", 0) == 0) return Lang::Spanish;
    if (code.rfind("ca", 0) == 0) return Lang::Catalan;
    return Lang::English;
}

Lang systemLanguage() {
    Lang found = Lang::English;
    if (SDL_Locale* locales = SDL_GetPreferredLocales()) {
        for (SDL_Locale* l = locales; l->language; l++) {
            const std::string code = l->language;
            if (code == "es" || code == "ca") {
                found = langFromCode(code);
                break;
            }
            if (code == "en") break;
        }
        SDL_free(locales);
    }
    return found;
}

const char* T(const char* english) {
    if (g_lang == Lang::English || english == nullptr || english[0] == '\0') return english;
    const auto& map = rows();
    const auto it = map.find(english);
    if (it == map.end()) {
        g_missing.insert(english);
        return english;
    }
    const char* text = g_lang == Lang::Spanish ? it->second->es : it->second->ca;
    return text ? text : english;
}

const char* T(const std::string& english) {
    // The result points into the table (or at `english` when there is no translation,
    // so the caller's string must outlive its use, as with c_str()).
    return T(english.c_str());
}

const char* TKnown(const std::string& text) {
    if (g_lang == Lang::English) return text.c_str();
    const auto it = rows().find(text);
    if (it == rows().end()) return text.c_str();
    const char* t = g_lang == Lang::Spanish ? it->second->es : it->second->ca;
    return t ? t : text.c_str();
}

std::string TF(const char* englishFormat, ...) {
    const char* format = T(englishFormat);
    va_list args;
    va_start(args, englishFormat);
    va_list copy;
    va_copy(copy, args);
    const int n = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    std::string out;
    if (n > 0) {
        std::vector<char> buf(static_cast<size_t>(n) + 1);
        std::vsnprintf(buf.data(), buf.size(), format, args);
        out.assign(buf.data(), static_cast<size_t>(n));
    }
    va_end(args);
    return out;
}

const std::set<std::string>& missingTranslations() { return g_missing; }
