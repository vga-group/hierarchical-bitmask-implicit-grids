#include "translator.hh"

namespace
{

// TODO: Make it possible to hide this message.
void warn_missing(const std::string& str, const std::string& lang_code)
{
    printf(
        "Missing %s translation for \"%s\"\n",
        lang_code.c_str(),
        str.c_str()
    );
}

}

namespace rb
{

translator::translator(const std::string& lang_code)
: lang_code(lang_code)
{
}

void translator::copy_translations_from(const translator& other)
{
    for(const auto& [lc, tr]: other.translations)
        for(const auto& [from, to]: tr)
            translations[lc][from] = to;
}

void translator::set_language(const std::string& lang_code)
{
    this->lang_code = lang_code;
}

const std::string& translator::get_language() const
{
    return lang_code;
}

std::string translator::_(const std::string& str) const
{
    if(lang_code == "en") return str;

    auto lang_it = translations.find(lang_code);
    if(lang_it == translations.end())
    {
        warn_missing(str, lang_code);
        return str;
    }

    auto tr_it = lang_it->second.find(str);
    if(tr_it == lang_it->second.end())
    {
        warn_missing(str, lang_code);
        return str;
    }

    return tr_it->second;
}

std::string translator::operator()(const std::string& str) const
{ return _(str); }

void translator::translate(const std::string& ) {}
bool translator::find_lang_str(std::string&, const std::string&)
{
    return false;
}

}
