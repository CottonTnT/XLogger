#pragma once
#include "Logger.h"

#include <Singleton.hpp>

#include <string>

namespace X {

class LoggerFactory {
public:
    static auto getLogger(std::string name)
        -> LoggerSPtr;
};

}