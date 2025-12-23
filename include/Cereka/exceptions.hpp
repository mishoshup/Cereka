
#pragma once

#include <cstdarg>
#include <exception>
#include <string>
#include <vector>

namespace cereka::engine {

/**
 * Base class for all the errors encountered by the engine.
 * It provides a field for storing custom messages related to the actual
 * error.
 */
class Error : public std::exception {
public:
  std::string message;

  Error() : message() {}

  explicit Error(const std::string &msg) : message(msg) {}

  explicit Error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    int size = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);

    std::vector<char> buf(size + 1);

    va_start(args, fmt);
    std::vsnprintf(buf.data(), buf.size(), fmt, args);
    va_end(args);

    message = std::string(buf.data());
  }

  ~Error() noexcept override {}

  const char *what() const noexcept override { return message.c_str(); }
};

/**
 * Alias Error as error to be consistent
 * accessing error when writing code.
 */
using error = Error;

} // namespace cereka::engine
