#ifndef RAYBASE_TRANSLATOR_HH
#define RAYBASE_TRANSLATOR_HH
#include <string>
#include <unordered_map>

namespace rb
{

class translator
{
public:
    translator(const std::string& lang_code = "en");

    void copy_translations_from(const translator& other);

    void set_language(const std::string& lang_code);
    const std::string& get_language() const;

    template<typename... Args>
    void translate(
        const std::string& from,
        const std::string& to_lang,
        const std::string& to_str,
        const Args&... args
    ){
        translations[to_lang][from] = to_str;
        translate(from, args...);
    }

    // Uses translation table, the input string must be English string.
    // Returns the given string if there is no such translation.
    std::string _(const std::string& str) const;
    std::string operator()(const std::string& str) const;

    // Picks the correct translation from the given set, falling back to
    // English if present.
    template<typename... Args>
    std::string _(
        const std::string& cd_lang_code,
        const std::string& cd_str,
        const Args&... args
    ) const
    {
        std::string ret;
        if(find_lang_str(ret, lang_code, cd_lang_code, cd_str, args...))
            return ret;
        if(
            lang_code != "en" &&
            find_lang_str(ret, "en", cd_lang_code, cd_str, args...)
        ) return _(ret);
        return ret;
    }
    template<typename... Args>
    std::string operator()(const Args&... args) const
    { return _(args...); }

private:
    std::unordered_map<
        std::string /* lang_code */,
        std::unordered_map<
            std::string /* from */,
            std::string /* to */
        >
    > translations;
    std::string lang_code;

    void translate(const std::string& from);
    static bool find_lang_str(std::string&, const std::string&);

    template<typename... Args>
    static bool find_lang_str(
        std::string& ret,
        const std::string& lang_code,
        const std::string& cd_lang_code,
        const std::string& cd_str,
        Args... args
    ){
        if(cd_lang_code == lang_code)
        {
            ret = cd_str;
            return true;
        }
        return find_lang_str(ret, lang_code, args...);
    }
};

}

#endif
