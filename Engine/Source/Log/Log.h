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

#define DEFINE_LOG_CATEGORY_STATIC(logName)  \
    namespace                                \
    {                                        \
        const LogCategory logName(#logName); \
    }

struct LogCategory
{
    explicit LogCategory(const std::string& name)
        : Name(name)
    {
    }

    std::string GetName() const { return Name; }

private:
    std::string Name;
};

class Log final : public NonCopyable
{
public:
    static Log& Get()
    {
        static Log instance;
        return instance;
    }

    void LogMsg(const LogCategory& category, LogVerbosity verbosity, const char* message) const;
    void LogMsg(const LogCategory& category, LogVerbosity verbosity, const std::string& message) const;

private:
    Log();
    ~Log();

    struct Impl;
    std::unique_ptr<Impl> m_pImpl;
};
}  // namespace Scald