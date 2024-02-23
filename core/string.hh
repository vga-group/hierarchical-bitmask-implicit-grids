#ifndef RAYBASE_STRING_HH
#define RAYBASE_STRING_HH
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>

namespace rb
{

std::vector<std::string> split(const std::string_view& view, const char* separator=" ");
std::string_view strip(const std::string_view& view, const char* remove = " \t");
bool ends_with(std::string_view str, std::string_view end);
std::string indent(const std::string_view& str, const char* with = "\t");

std::string escape_string(const std::string& str);
std::string unescape_string(const std::string& str);

// Returns true if there was whitespace.
bool skip_whitespace(const char*& s);
bool advance_prefix(const char*& s, const char* prefix);
bool parse_bool(const char*& s, bool& b);
bool parse_int(const char*& s, int64_t& i);
bool parse_uint(const char*& s, uint64_t& i);
bool parse_float(const char*& s, double& f);
bool parse_string(const char*& s, std::string& str);

}

#endif
