#pragma once

#include <iostream>

namespace X::detail {

/* ======================== StdoutAppender ======================== */
class StdoutAppender {
public:
    void append(std::string_view msg);
};

inline void StdoutAppender::append(std::string_view msg)
{
    std::cout << msg;
    std::cout.flush();
}

// todo
class SocketAppender;

// todo
class SystemAppender;

} // namespace X::detail
