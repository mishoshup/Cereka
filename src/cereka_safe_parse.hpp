#pragma once
#include <charconv>
#include <expected>
#include <string>
#include <cstdlib>

namespace cereka {

// ---------------------------------------------------------------------------
// Safe numeric parsers — return expected<T, error_string> instead of
// throwing or silently producing undefined behavior on malformed input.
//
// These replace unguarded std::stoi / std::stof calls throughout the engine.
// ---------------------------------------------------------------------------

inline std::expected<float, std::string> safe_stof(const std::string &s) noexcept
{
    if (s.empty())
        return std::unexpected(std::string("empty string"));

    char *end = nullptr;
    float val = std::strtof(s.c_str(), &end);
    if (end == s.c_str())
        return std::unexpected(std::string("invalid float: '") + s + "'");
    return val;
}

inline std::expected<int, std::string> safe_stoi(const std::string &s) noexcept
{
    if (s.empty())
        return std::unexpected(std::string("empty string"));

    int val = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec != std::errc{})
        return std::unexpected(std::string("invalid int: '") + s + "'");
    return val;
}

inline std::expected<unsigned long long, std::string>
safe_stoull(const std::string &s) noexcept
{
    if (s.empty())
        return std::unexpected(std::string("empty string"));

    unsigned long long val = 0;
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), val);
    if (ec != std::errc{})
        return std::unexpected(std::string("invalid unsigned: '") + s + "'");
    return val;
}

}  // namespace cereka
