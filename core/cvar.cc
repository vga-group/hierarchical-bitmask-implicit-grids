#include "cvar.hh"
#include "string.hh"
#include <algorithm>

namespace rb
{

cvar_table& get_global_cvars()
{
    static cvar_table global_table;
    return global_table;
}

cvar_table prepare_subset_table(const cvar_table& src, const std::string& cvar_name)
{
    cvar_table subset;

    auto it = src.find(cvar_name);
    if(it == src.end()) return subset;

    const cvar& cv = it->second;
    subset.emplace(cvar_name, cv);

    if(auto* vec = std::get_if<std::vector<std::string>>(&cv.value))
    {
        for(const std::string& suffix: *vec)
            apply_subset_table(prepare_subset_table(src, cvar_name + "." + suffix), subset);
    }

    return subset;
}

void apply_subset_table(cvar_table&& subset, cvar_table& dst)
{
    for(auto& pair: subset)
        dst.insert_or_assign(pair.first, std::move(pair.second));
}

std::string dump_cvar_table()
{
    return dump_cvar_table(get_global_cvars());
}

std::string dump_cvar(const std::string& name, const std::string& short_name, const cvar& cv, const cvar_table& ct)
{
    switch(cv.t)
    {
    case cvar::BOOLEAN:
        {
        std::string str = "bool " + short_name;
        if(auto* v = std::get_if<bool>(&cv.value))
            str += " = " + std::string(*v ? "true" : "false");
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::STRING:
        {
        std::string str = "string " + short_name;
        if(auto* v = std::get_if<std::string>(&cv.value))
            str += " = " + escape_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::STRUCT:
        {
        std::string str = "struct " + short_name + " {\n";
        const std::vector<std::string>& members = std::get<std::vector<std::string>>(cv.value);
        for(const std::string& suffix: members)
        {
            const std::string member_name = name + "." + suffix;
            auto it = ct.find(member_name);
            std::string entry;
            if(it != ct.end())
                entry = dump_cvar(member_name, suffix, it->second, ct);
            else entry = "UNKNOWN" + member_name;
            entry += "\n";
            str += indent(entry);
        }
        str += "}";
        return str;
        }
    case cvar::ENUM:
        {
        std::string str = "enum " + short_name;
        if(auto* v = std::get_if<enum_cvar_key>(&cv.value))
        {
            std::string enum_name = "##UNKNOWN##";
            for(auto& pair: v->key)
            {
                if(pair.second == v->value)
                    enum_name = pair.first;
            }
            str += " = " + enum_name;
        }
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::U8:
        {
        std::string str = "u8 " + short_name;
        if(auto* v = std::get_if<uint8_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::U16:
        {
        std::string str = "u16 " + short_name;
        if(auto* v = std::get_if<uint16_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::U32:
        {
        std::string str = "u32 " + short_name;
        if(auto* v = std::get_if<uint32_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::U64:
        {
        std::string str = "u64 " + short_name;
        if(auto* v = std::get_if<uint64_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::I8:
        {
        std::string str = "i8 " + short_name;
        if(auto* v = std::get_if<int8_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::I16:
        {
        std::string str = "i16 " + short_name;
        if(auto* v = std::get_if<int16_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::I32:
        {
        std::string str = "i32 " + short_name;
        if(auto* v = std::get_if<int32_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::I64:
        {
        std::string str = "i64 " + short_name;
        if(auto* v = std::get_if<int64_t>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::F32:
        {
        std::string str = "f32 " + short_name;
        if(auto* v = std::get_if<float>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    case cvar::F64:
        {
        std::string str = "f64 " + short_name;
        if(auto* v = std::get_if<double>(&cv.value))
            str += " = " + std::to_string(*v);
        else if(cv.default_value_string)
            str += " = " + *cv.default_value_string;
        return str;
        }
    }
    return "UNKNOWN";
}

std::string dump_cvar_table(const cvar_table& ct)
{
    std::string guide;
    guide += "Available CVARs:\n";
    for(const auto& [name, cv]: ct)
    {
        // We don't print member variables because they're included in the type
        // of a top-level variable.
        if(cv.is_member)
            continue;
        guide += indent(dump_cvar(name, name, cv, ct))+"\n";
    }
    return guide;
}

bool cvar_changed(const std::string& name, cvar_table& ct)
{
    auto it = ct.find(name);
    if(it == ct.end()) return false;
    return it->second.changed;
}

bool cvar_changed(const std::string& name)
{
    return cvar_changed(name, get_global_cvars());
}

std::string clean_cvar_name(const char* name)
{
    std::string clean_name;
    // Replace all -> with . and remove everything preceding a ( and succeeding a )
    // and remove whitespace.
    while(*name)
    {
        switch(*name)
        {
        case '(':
            clean_name.clear();
            name++;
            break;
        case ')':
            return clean_name;
        case ' ':
        case '\n':
        case '\t':
            name++;
            break;
        case '-':
            if(name[1] == '>')
            {
                clean_name += ".";
                name += 2;
            }
            else clean_name += *name;
            name++;
            break;
        default:
            clean_name += *name;
            name++;
            break;
        }
    }

    return clean_name;
}

bool parse_cvar(const std::string& name, cvar& cv, cvar_table& ct, const char*& str, uint64_t& mask)
{
    skip_whitespace(str);
    cv.value_overrides = true;

    mask |= cv.category_mask;
    switch(cv.t)
    {
    case cvar::BOOLEAN:
        {
        bool b;
        if(!parse_bool(str, b))
            return false;
        cv.value = b;
        return cv.changed = true;
        }
    case cvar::STRING:
        {
        std::string s;
        if(!parse_string(str, s))
            return false;
        cv.value = s;
        return cv.changed = true;
        }
    case cvar::STRUCT:
        {
        // We allow an arbitrary name for the struct.
        while(std::isalnum(*str) || *str == ':' || *str == '_') str++;
        if(*str != '(' && *str != '{')
            return false;
        str++;
        const std::vector<std::string>& members = std::get<std::vector<std::string>>(cv.value);
        for(const std::string& suffix: members)
        {
            str += strspn(str, " \t\n,");
            const std::string member_name = name + "." + suffix;
            auto it = ct.find(member_name);
            if(it != ct.end())
            {
                if(!parse_cvar(member_name, it->second, ct, str, mask))
                    return false;
            }
        }
        skip_whitespace(str);
        cv.changed = true;
        if(*str != ')' && *str != '}')
            return false;
        str++;
        return true;
        }
    case cvar::ENUM:
        {
        enum_cvar_key& ck = std::get<enum_cvar_key>(cv.value);

        size_t len = strcspn(str, " \t\n,");
        std::string name(str, len);

        for(const auto& pair: ck.key)
        {
            if(pair.first == name)
            {
                ck.value = pair.second;
                str += len;
                return cv.changed = true;
            }
        }
        return false;

        }
    case cvar::U8:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = uint8_t(std::clamp(v, int64_t(0), int64_t(UINT8_MAX)));
        return cv.changed = true;
        }
    case cvar::U16:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = uint16_t(std::clamp(v, int64_t(0), int64_t(UINT16_MAX)));
        return cv.changed = true;
        }
    case cvar::U32:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = uint32_t(std::clamp(v, int64_t(0), int64_t(UINT32_MAX)));
        return cv.changed = true;
        }
    case cvar::U64:
        {
        uint64_t v;
        if(!parse_uint(str, v))
            return false;
        cv.value = v;
        return cv.changed = true;
        }
    case cvar::I8:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = int8_t(std::clamp(v, int64_t(INT8_MIN), int64_t(INT8_MAX)));
        return cv.changed = true;
        }
    case cvar::I16:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = int16_t(std::clamp(v, int64_t(INT16_MIN), int64_t(INT16_MAX)));
        return cv.changed = true;
        }
    case cvar::I32:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = int32_t(std::clamp(v, int64_t(INT32_MIN), int64_t(INT32_MAX)));
        return cv.changed = true;
        }
    case cvar::I64:
        {
        int64_t v;
        if(!parse_int(str, v))
            return false;
        cv.value = v;
        return cv.changed = true;
        }
    case cvar::F32:
        {
        double v;
        if(!parse_float(str, v))
            return false;
        cv.value = float(v);
        return cv.changed = true;
        }
    case cvar::F64:
        {
        double v;
        if(!parse_float(str, v))
            return false;
        cv.value = double(v);
        return cv.changed = true;
        }
    }
    return false;
}

bool parse_cvar(const std::string& name, cvar& cv, cvar_table& ct, const std::string& str, uint64_t& mask)
{
    const char* s = str.c_str();
    return parse_cvar(name, cv, ct, s, mask);
}

uint64_t run_cvar_command(const std::string& command, cvar_table& ct)
{
    const char* str = command.c_str();

    skip_whitespace(str);
    size_t name_len = strcspn(str, " \t\n=");
    std::string cvar_name = clean_cvar_name(std::string(str, name_len).c_str());
    if(cvar_name.size() == 0)
        return 0;

    str += name_len;
    str += strspn(str, " \t\n=");

    if(*str == 0) // May be some command.
    {
        auto it = ct.find(cvar_name);
        if(it != ct.end())
        {
            std::cout << dump_cvar(cvar_name, cvar_name, it->second, ct) << std::endl;
            return 0;
        }
        else
        {
            if(cvar_name == "help")
            {
                std::cout << dump_cvar_table(ct);
                return 0;
            }
            else std::cerr << "No such cvar: " << cvar_name << std::endl;
            return 0;
        }
    }

    auto it = ct.find(cvar_name);
    if(it == ct.end())
    {
        std::cerr << "No such cvar: " << cvar_name << std::endl;
        return 0;
    }

    cvar_table alt = prepare_subset_table(ct, cvar_name);
    uint64_t mask = 0;
    if(!parse_cvar(cvar_name, alt.at(cvar_name), alt, str, mask))
    {
        std::cerr << "Failed to parse value." << std::endl;
        return 0;
    }

    skip_whitespace(str);
    if(*str != '\0')
    {
        std::cerr << "Garbage at end of line." << std::endl;
        return 0;
    }

    apply_subset_table(std::move(alt), ct);
    return mask;
}

uint64_t run_cvar_command(const std::string& command)
{
    return run_cvar_command(command, get_global_cvars());
}

void begin_stdin_cmdline()
{
    std::ios_base::sync_with_stdio(false);
    std::cout << ">>> " << std::flush;
}

bool try_stdin_cmdline()
{
    std::string cmdline;
    static std::stringstream in;
    char c;
    bool finished = false;
    while(std::cin.readsome(&c, 1))
    {
        if(c == '\n')
        {
            cmdline = in.str();
            in.str("");
            finished = true;
            break;
        }
        in << c;
    }
    if(!finished)
        return false;

    bool ret = run_cvar_command(cmdline);

    begin_stdin_cmdline();
    return ret;
}

cvar* insert_cvar(cvar::type t, const std::string& name, uint64_t category, cvar_table& table)
{
    std::vector<std::string> parts = split(name, ".");
    std::string prefix;

    cvar* added = nullptr;
    for(size_t i = 0; i < parts.size(); ++i)
    {
        const std::string& p = parts[i];

        bool member = false;
        if(i == 0)
            prefix = p;
        else
        {
            prefix += "."+p;
            member = true;
        }

        cvar cv;
        cv.t = t;
        cv.changed = true;
        cv.category_mask = category;

        cv.value = std::monostate();
        cv.value_overrides = false;
        cv.is_member = member;

        if(i != parts.size()-1)
        {
            cv.t = cvar::STRUCT;
            cv.value = std::vector<std::string>{parts[i+1]};
        }

        auto it = table.find(prefix);
        if(it != table.end())
        {
            cvar& ocv = it->second;
            ocv.category_mask |= category;
            if(cv.t != ocv.t || cv.is_member != ocv.is_member)
                throw cvar_redefinition_error("Redefining CVAR " + prefix + " with incompatible type!");

            if(auto* ovec = std::get_if<std::vector<std::string>>(&ocv.value))
            {
                auto& vec = std::get<std::vector<std::string>>(cv.value);
                for(const std::string& member: vec)
                {
                    if(std::find(ovec->begin(), ovec->end(), member) == ovec->end())
                        ovec->push_back(member);
                }
            }
            added = &ocv;
        }
        else
        {
            added = &table.emplace(prefix, std::move(cv)).first->second;
        }
    }
    return added;
}

template<>
void register_cvar<bool>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::BOOLEAN, name, category, table); }

template<>
void register_cvar<std::string>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::STRING, name, category, table); }

template<>
void register_cvar<uint8_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::U8, name, category, table); }

template<>
void register_cvar<uint16_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::U16, name, category, table); }

template<>
void register_cvar<uint32_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::U32, name, category, table); }

template<>
void register_cvar<uint64_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::I8, name, category, table); }

template<>
void register_cvar<int16_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::I16, name, category, table); }

template<>
void register_cvar<int32_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::I32, name, category, table); }

template<>
void register_cvar<int64_t>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::I64, name, category, table); }

template<>
void register_cvar<float>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::F32, name, category, table); }

template<>
void register_cvar<double>(const std::string& name, uint64_t category, cvar_table& table)
{ insert_cvar(cvar::F64, name, category, table); }

}
