#ifndef RAYBASE_CVAR_TCC
#define RAYBASE_CVAR_TCC
#include "cvar.hh"
#include "log.hh"
#include "string.hh"

namespace rb
{

template<typename T>
void register_cvar(const std::string& name, uint64_t category, cvar_table& table);

template<typename T>
T get_struct_cvar_value(const std::string& name, cvar& cv, cvar_table& ct, const T& fallback);

template<typename T>
uint64_t set_cvar_value(const std::string& name, cvar& cv, cvar_table& ct, const T& t, bool default_value)
{
    if(cv.value_overrides && default_value)
        return 0;

    cv.value_overrides = !default_value;
    cv.changed = true;

    if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, char*>)
    {
        if(cv.t == cvar::STRING) cv.value = std::string(t);
        else
        {
            uint64_t mask = cv.category_mask;
            parse_cvar(name, cv, ct, std::string(t), mask);
            return mask;
        }
    }
    else switch(cv.t)
    {
    case cvar::BOOLEAN:
        cv.value = (bool)t;
        break;
    case cvar::STRING:
        if constexpr(!std::is_enum_v<T>)
            cv.value = to_string(t);
        break;
    case cvar::STRUCT:
        break;
    case cvar::ENUM:
        {
            enum_cvar_key& ck = std::get<enum_cvar_key>(cv.value);
            if(std::type_index(typeid(t)) == ck.type)
                ck.value = size_t(t);
        }
        break;
    case cvar::U8:
        cv.value = uint8_t(t);
        break;
    case cvar::U16:
        cv.value = uint16_t(t);
        break;
    case cvar::U32:
        cv.value = uint32_t(t);
        break;
    case cvar::U64:
        cv.value = uint64_t(t);
        break;
    case cvar::I8:
        cv.value = int8_t(t);
        break;
    case cvar::I16:
        cv.value = int16_t(t);
        break;
    case cvar::I32:
        cv.value = int32_t(t);
        break;
    case cvar::I64:
        cv.value = int64_t(t);
        break;
    case cvar::F32:
        cv.value = float(t);
        break;
    case cvar::F64:
        cv.value = double(t);
        break;
    }
    return cv.category_mask;
}

template<typename T>
uint64_t set_cvar(const std::string& name, cvar_table& ct, const T& t)
{
    auto it = ct.find(name);
    if(it == ct.end())
    {
        register_cvar<T>(name, 1, ct);
        return set_cvar(name, ct, t);
    }
    return set_cvar_value(name, it->second, ct, t);
}

template<typename T>
uint64_t set_cvar(const std::string& name, const T& t)
{
    return set_cvar(name, get_global_cvars(), t);
}

template<typename T>
T get_cvar(const std::string& name, cvar_table& ct)
{
    auto it = ct.find(name);
    if(it == ct.end())
        return T();

    cvar& cv = it->second;
    if(std::holds_alternative<std::monostate>(cv.value))
        return T();

    if constexpr(
        std::is_same_v<T, bool> ||
        std::is_same_v<T, std::string> ||
        std::is_same_v<T, uint8_t> ||
        std::is_same_v<T, uint16_t> ||
        std::is_same_v<T, uint32_t> ||
        std::is_same_v<T, uint64_t> ||
        std::is_same_v<T, int8_t> ||
        std::is_same_v<T, int16_t> ||
        std::is_same_v<T, int32_t> ||
        std::is_same_v<T, int64_t> ||
        std::is_same_v<T, float> ||
        std::is_same_v<T, double>
    ) return std::get<T>(cv.value);
    else return get_struct_cvar_value<T>(name, cv, ct, T());
}

template<typename T>
T get_cvar(const std::string& name)
{
    return get_cvar<T>(name, get_global_cvars());
}

template<const char* name, const char* ival_str, uint64_t category, typename T>
struct cvar_registrar
{
    struct core {
        core() {
            std::string clean_name = clean_cvar_name(name);
            cvar_table& table = get_global_cvars();
            register_cvar<T>(clean_name, category, table);
            cvar& cv = table[clean_name];
            cv.default_value_string = ival_str;

            cvar_table alt = prepare_subset_table(table, clean_name);
            uint64_t mask = 0;
            if(parse_cvar(clean_name, alt.at(clean_name), alt, ival_str, mask))
            {
                apply_subset_table(std::move(alt), table);
                cv.value_overrides = false;
            }
        }
    };
    static inline core c;
};

template<typename T, typename U>
void fetch_cvar(T& var, const char* name, uint64_t category, const char* ival_str, const U& initial_value, cvar_table& table)
{
    std::string clean_name = clean_cvar_name(name);
    auto it = table.find(clean_name);
    if(it == table.end())
        register_cvar<T>(clean_name, category, table);
    table[clean_name].default_value_string = ival_str;

    cvar& cv = it->second;
    cv.changed = false;
    if(!std::holds_alternative<std::monostate>(cv.value))
    {
        if constexpr(std::is_class_v<T>)
        {
            var = get_struct_cvar_value<T>(clean_name, cv, table, initial_value);
            return;
        }
        else if(cv.value_overrides)
        {
            var = get_cvar<T>(clean_name, table);
            return;
        }
    }
    set_cvar_value<T>(clean_name, cv, table, initial_value, true);
    var = initial_value;
}

template<const char* name, const char* ival_str, uint64_t category, typename T, typename U>
void fetch_cvar_global(T& var, const U& initial_value)
{
    // Just instantiate the registrar so we get the names of all cvars even
    // before they're used.
    (void)cvar_registrar<name, ival_str, category, T>::c;
    fetch_cvar(var, name, category, ival_str, initial_value, get_global_cvars());
}

template<> void register_cvar<bool>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<std::string>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<uint8_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<uint16_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<uint32_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<uint64_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<int8_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<int16_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<int32_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<int64_t>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<float>(const std::string& name, uint64_t category, cvar_table& table);
template<> void register_cvar<double>(const std::string& name, uint64_t category, cvar_table& table);
}

#define RB_CVAR_TYPE_MEMBER(member_name) \
    rb::register_cvar<decltype(T::member_name)>(name+"."+#member_name, category, table);

#define RB_SET_MEMBER_CVAR(member_name) \
    { \
        std::string joint_name = name+"."+#member_name; \
        mask |= rb::set_cvar_value<decltype(T::member_name)>(joint_name, ct.at(joint_name), ct, t.member_name, default_value); \
    }

#define RB_GET_MEMBER_CVAR(member_name) \
    { \
        using U = decltype(T::member_name); \
        std::string joint_name = name+"."+#member_name; \
        cvar& cv = ct.at(joint_name); \
        if(!std::holds_alternative<std::monostate>(cv.value) && cv.value_overrides) \
        { \
            if constexpr( \
                std::is_same_v<U, bool> || \
                std::is_same_v<U, std::string> || \
                std::is_same_v<U, uint8_t> || \
                std::is_same_v<U, uint16_t> || \
                std::is_same_v<U, uint32_t> || \
                std::is_same_v<U, uint64_t> || \
                std::is_same_v<U, int8_t> || \
                std::is_same_v<U, int16_t> || \
                std::is_same_v<U, int32_t> || \
                std::is_same_v<U, int64_t> || \
                std::is_same_v<U, float> || \
                std::is_same_v<U, double> \
            ) ret.member_name = std::get<U>(cv.value); \
            else ret.member_name = get_struct_cvar_value<U>(joint_name, cv, ct, fallback.member_name); \
        } \
        else \
        { \
            set_cvar_value<U>(joint_name, cv, ct, fallback.member_name, true); \
        } \
    }

#define RB_CVAR_STRUCT(type, ...) \
    template<> inline void rb::register_cvar<type>(const std::string& name, uint64_t category, rb::cvar_table& table) \
    { \
        using T = type; \
        RB_FOR_EACH(RB_CVAR_TYPE_MEMBER, __VA_ARGS__); \
    } \
    template<> inline uint64_t rb::set_cvar_value<type>(const std::string& name, cvar& cv, cvar_table& ct, const type& t, bool default_value) \
    { \
        using T = type; \
        if(cv.value_overrides && default_value) \
            return 0; \
        cv.value_overrides = !default_value; \
        if(cv.t != cvar::STRUCT) \
            return 0; \
        else \
        { \
            uint64_t mask = cv.category_mask; \
            RB_FOR_EACH(RB_SET_MEMBER_CVAR, __VA_ARGS__); \
            return mask; \
        } \
    } \
    template<> inline type rb::get_struct_cvar_value<type>(const std::string& name, cvar& cv, cvar_table& ct, const type& fallback) \
    { \
        using T = type; \
        type ret = fallback; \
        if(cv.t != cvar::STRUCT) return ret; \
        RB_FOR_EACH(RB_GET_MEMBER_CVAR, __VA_ARGS__); \
        return ret; \
    }

#define RB_CVAR_ENUM_VALUE(value_name) \
    ck.key.emplace_back(#value_name, size_t(E::value_name));

#define RB_CVAR_ENUM(T, ...) \
    template<> inline void rb::register_cvar<T>(const std::string& name, uint64_t category, rb::cvar_table& table) \
    { \
        enum_cvar_key ck; \
        ck.type = typeid(T); \
        using E = T; \
        RB_FOR_EACH(RB_CVAR_ENUM_VALUE, __VA_ARGS__); \
        ck.value = ck.key[0].second; \
        insert_cvar(cvar::ENUM, name, category, table)->value = ck; \
    } \
    template<> inline T rb::get_cvar(const std::string& name, cvar_table& ct) \
    { \
        auto it = ct.find(name); \
        if(it == ct.end()) \
            return T(); \
        cvar& cv = it->second; \
        if(std::holds_alternative<std::monostate>(cv.value)) \
            return T(); \
        if(enum_cvar_key* ck = std::get_if<enum_cvar_key>(&cv.value)) \
        { \
            if(ck->type == std::type_index(typeid(T))) \
            { \
                return T(ck->value); \
            } \
        } \
        return T(); \
    }

RB_CVAR_STRUCT(rb::vec4, x, y, z, w)
RB_CVAR_STRUCT(rb::vec3, x, y, z)
RB_CVAR_STRUCT(rb::vec2, x, y)
RB_CVAR_STRUCT(rb::ivec4, x, y, z, w)
RB_CVAR_STRUCT(rb::ivec3, x, y, z)
RB_CVAR_STRUCT(rb::ivec2, x, y)
RB_CVAR_STRUCT(rb::uvec4, x, y, z, w)
RB_CVAR_STRUCT(rb::uvec3, x, y, z)
RB_CVAR_STRUCT(rb::uvec2, x, y)
#endif
