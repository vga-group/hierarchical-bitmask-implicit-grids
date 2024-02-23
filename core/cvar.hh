#ifndef RAYBASE_CVAR_HH
#define RAYBASE_CVAR_HH
#include <cstdio>
#include <cstdint>
#include <utility>
#include <vector>
#include <string>
#include <map>
#include <variant>
#include <stdexcept>
#include <typeindex>
#include <optional>
#include "macro_hacks.hh"
#include "bitset.hh"

namespace rb
{

struct enum_cvar_key
{
    std::type_index type = typeid(enum_cvar_key);
    std::vector<std::pair<std::string_view, size_t>> key;
    size_t value;
};

struct cvar
{
    enum type
    {
        BOOLEAN,
        STRING,
        STRUCT,
        ENUM,
        U8,
        U16,
        U32,
        U64,
        I8,
        I16,
        I32,
        I64,
        F32,
        F64
    };
    type t;

    bool is_member;
    bool changed;
    uint64_t category_mask; // Defaults to 1.
    // Default values are not normative and won't override hardcoded values.
    bool value_overrides;
    using value_type = std::variant<
        std::monostate,
        bool,
        std::string, // STRING
        std::vector<std::string>, // struct members
        enum_cvar_key,
        uint8_t,
        uint16_t,
        uint32_t,
        uint64_t,
        int8_t,
        int16_t,
        int32_t,
        int64_t,
        float,
        double
    >;
    value_type value;
    // Sometimes, we want to be able to print the default value before it has
    // even been evaluated yet. This is used as a placeholder for that.
    std::optional<std::string> default_value_string;
};

using cvar_table = std::map<std::string, cvar>;

cvar_table& get_global_cvars();

cvar_table prepare_subset_table(const cvar_table& src, const std::string& cvar_name);
void apply_subset_table(cvar_table&& subset, cvar_table& dst);

std::string dump_cvar_table();
std::string dump_cvar_table(const cvar_table& ct);

struct cvar_redefinition_error: public std::runtime_error
{ using std::runtime_error::runtime_error; };

template<typename T>
uint64_t set_cvar(const std::string& name, cvar_table& ct, const T& t);

template<typename T>
uint64_t set_cvar(const std::string& name, const T& t);

template<typename T>
T get_cvar(const std::string& name, cvar_table& ct);

template<typename T>
T get_cvar(const std::string& name);

bool cvar_changed(const std::string& name, cvar_table& ct);
bool cvar_changed(const std::string& name);

std::string clean_cvar_name(const char* name);

bool parse_cvar(const std::string& name, cvar& cv, cvar_table& ct, const char*& str, uint64_t& mask);
bool parse_cvar(const std::string& name, cvar& cv, cvar_table& ct, const std::string& str, uint64_t& mask);

// These return the combined category mask of all cvars that got changed.
uint64_t run_cvar_command(const std::string& command, cvar_table& ct);
uint64_t run_cvar_command(const std::string& command);

// DON'T USE THIS DIRECTLY!
cvar* insert_cvar(cvar::type t, const std::string& name, uint64_t category, cvar_table& table);

void begin_stdin_cmdline();
bool try_stdin_cmdline();

// You can use the CVARs for making things optional with minimum effort. Unlike
// many other systems, the one in Raybase also doesn't force you to use global
// variables.
#define RB_CVAR(var, ...) RB_NCCVAR(var, #var, 1, __VA_ARGS__)
// This one also specifies the category mask.
#define RB_CCVAR(var, category, ...) RB_NCCVAR(var, #var, category, __VA_ARGS__)

// You can use this if you don't want auto-deduced variable names. name must be
// a literal.
#define RB_NCVAR(var, name, ...) RB_NCCVAR(var, name, 1, __VA_ARGS__)
#define RB_NCCVAR(var, name, category, ...) {\
    static constexpr char cvar_name[] = name; \
    static constexpr char ival_str[] = #__VA_ARGS__; \
    rb::fetch_cvar_global<cvar_name, ival_str, category>(var, __VA_ARGS__); \
}

// Local CVAR
#define RB_LCVAR(var, name, table, ...) RB_LNCCVAR(var, #var, 1, table, __VA_ARGS__)
// Local, renamed CVAR
#define RB_LNCVAR(var, name, table, ...) RB_LNCCVAR(var, name, 1, table, __VA_ARGS__)
#define RB_LNCCVAR(var, name, category, table, ...) {\
    rb::fetch_cvar(var, name, category, #__VA_ARGS__, __VA_ARGS__, table); \
}

}

// There's two more macros that could be interesting:
//
// RB_CVAR_STRUCT(type_name, member1, member2, ...),
// which is used for adding support for structs as cvar types. You simply give
// it the name of the struct and list its members, and after that, the struct
// works as a CVAR type.
//
// RB_CVAR_ENUM(type_name, value1, value2, ...),
// which is used for adding support for enums as cvar types. You simply list the
// possible values the enum may have.
#include "cvar.tcc"
#endif
