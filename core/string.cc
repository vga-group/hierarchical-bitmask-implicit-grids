#include "string.hh"
#include <cstring>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace rb
{

std::vector<std::string> split(const std::string_view& view, const char* separator)
{
    std::vector<std::string> parts;
    std::string_view::size_type pos = 0;
    std::string_view::size_type skip = strlen(separator);
    for(;;)
    {
        std::string_view::size_type end_pos = view.find(separator, pos);
        parts.push_back(std::string(view.substr(pos, end_pos-pos)));
        if(end_pos == std::string_view::npos) break;
        pos = end_pos + skip;
    }

    return parts;
}

std::string_view strip(const std::string_view& view, const char* remove)
{
    std::string_view::size_type begin = view.find_first_not_of(remove);
    std::string_view::size_type end = view.find_last_not_of(remove);
    if(begin == std::string_view::npos) begin = 0;
    if(end != std::string_view::npos) end++;
    return view.substr(begin, end);
}

bool ends_with(std::string_view str, std::string_view end)
{
    if(end.size() > str.size())
        return false;
    return std::equal(end.rbegin(), end.rend(), str.rbegin());
}

std::string indent(const std::string_view& str, const char* with)
{
    std::string out;
    bool indent_due = true;
    for(char c: str)
    {
        if(indent_due && c != '\n' && c != '\r')
        {
            out += with;
            indent_due = false;
        }
        out += c;
        if(c == '\n' || c == '\r') indent_due = true;
    }
    return out;
}

std::string escape_string(const std::string& str)
{
    std::stringstream out;
    out.imbue(std::locale());
    out << "\"";
    for(char c: str)
    {
        switch(c)
        {
        case '\"':
        case '\\': out << '\\' << c; break;
        case '\a': out << "\\a"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        case '\v': out << "\\v"; break;
        default:
            if(c < ' ')
                out << '\\' << std::oct
                    << std::setfill('0') << std::setw(3) << (unsigned)c;
            else out << c;
            break;
        }
    }
    out << "\"";
    return out.str();
}

std::string unescape_string(const std::string& str)
{
    std::stringstream out;
    out.imbue(std::locale());

    bool escaped = false;
    unsigned octal_chain = 0;
    unsigned value = 0;
    for(unsigned i = 0; i < str.size(); ++i)
    {
        char c = str[i];
        if(escaped)
        {
            escaped = false;
            switch(c)
            {
            case '\"':
            case '\\': out << c; break;
            case 'a': out << '\a'; break;
            case 'b': out << '\b'; break;
            case 'f': out << '\f'; break;
            case 'n': out << '\n'; break;
            case 'r': out << '\r'; break;
            case 't': out << '\t'; break;
            case 'v': out << '\v'; break;
            default:
                if(c >= '0' && c <= '7')
                {
                    octal_chain++;
                    value = c - '0';
                }
                else throw std::runtime_error(
                    std::string("Unrecognized escape sequence ") + c
                );
                break;
            }
            continue;
        }

        if(octal_chain > 0)
        {
            if(octal_chain < 3 && c >= '0' && c <= '7')
            {
                octal_chain++;
                value = value * 8 + (c - '0');
                continue;
            }
            else
            {
                out << (char)value;
                octal_chain = 0;
            }
        }

        if(c == '\\') escaped = true;
        else if(c != '"') out << c;
    }

    return out.str();
}

bool skip_whitespace(const char*& s)
{
    bool skipped = false;
    while(*s && (*s == ' ' || *s == '\t' || *s == '\n'))
    {
        ++s;
        skipped = true;
    }
    return skipped;
}

bool advance_prefix(const char*& s, const char* prefix)
{
    size_t len = strlen(prefix);
    if(strncmp(prefix, s, len) == 0)
    {
        s += len;
        return true;
    }
    return false;
}

bool parse_bool(const char*& s, bool& b)
{
    if(advance_prefix(s, "true") || advance_prefix(s, "on"))
    {
        b = true;
        return true;
    }
    if(advance_prefix(s, "false") || advance_prefix(s, "off"))
    {
        b = false;
        return true;
    }
    return false;
}

bool parse_int(const char*& s, int64_t& i)
{
    char* end = nullptr;
    i = strtoll(s, &end, 0);
    if(end == s) return false;
    s = end;
    if(*s == 'u' || *s == 'U') s++;
    if(*s == 'l' || *s == 'L') s++;
    if(*s == 'l' || *s == 'L') s++;
    if(*s == 'u' || *s == 'U') s++;
    return true;
}

bool parse_uint(const char*& s, uint64_t& i)
{
    char* end = nullptr;
    i = strtoull(s, &end, 0);
    if(end == s) return false;
    s = end;
    if(*s == 'u' || *s == 'U') s++;
    if(*s == 'l' || *s == 'L') s++;
    if(*s == 'l' || *s == 'L') s++;
    if(*s == 'u' || *s == 'U') s++;
    return true;
}

bool parse_float(const char*& s, double& f)
{
    char* end = nullptr;
    f = strtod(s, &end);
    if(end == s) return false;
    s = end;
    if(*s == 'f' || *s == 'F') s++;
    return true;
}

bool parse_string(const char*& s, std::string& str)
{
    if(*s != '"')
        return false;

    int length = 1;
    while(s[length] != 0)
    {
        if(s[length] == '\\' && s[length+1] == '"')
            length+=2;
        else if(s[length] == '"')
            break;
        else length++;
    }
    if(s[length] != '"')
        return false;

    std::string segment(s, length+1);
    s += length+1;
    str = unescape_string(segment);
    return true;
}

}
