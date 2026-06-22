#pragma once

#include "Utility.h"

#include <cstdint>
#include <string>
#include <memory>

namespace Scald
{

enum class LogVerbosity : uint8_t
{
    NoLogging,
    Display,
    Warning,
    Error,
    Fatal
};

class Log final : public NonCopyable
{
public:
    static Log& Get()
    {
        static Log instance;
        return instance;
    }

    void LogMsg(LogVerbosity verbosity, const char* message) const;
    void LogMsg(LogVerbosity verbosity, const std::string& message) const;

private:
    Log();
    ~Log() = default;

    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};
}