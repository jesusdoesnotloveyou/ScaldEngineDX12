#include "Log.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <memory>
#include <unordered_map>

using namespace Scald;

namespace
{
    const std::unordered_map<LogVerbosity, spdlog::level::level_enum> c_verbosityMap = 
    {
        {LogVerbosity::NoLogging, spdlog::level::off},
        {LogVerbosity::Display, spdlog::level::info},
        {LogVerbosity::Warning, spdlog::level::warn},
        {LogVerbosity::Error, spdlog::level::err},
        {LogVerbosity::Fatal, spdlog::level::critical}
    };
}

// pImpl
struct Log::Impl
{
    Impl()
    {
        const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_logger = std::make_unique<spdlog::logger>("Win32Console", consoleSink);
    }

    void LogMsg(LogVerbosity verbosity, const char* message) const
    {
        if (verbosity == LogVerbosity::NoLogging) return;

        if (const auto spdLevel = c_verbosityMap.find(verbosity); spdLevel != c_verbosityMap.end())
        {
            m_logger->log(spdLevel->second, message);
        }
    }

private:
    std::unique_ptr<spdlog::logger> m_logger;
};

// Interface 
Log::Log() : m_pImpl(std::make_unique<Impl>()) {}

// For using unique_ptr for PIMPL 
Log::~Log() = default;  

// methods provide a level of indirection
void Log::LogMsg(LogVerbosity verbosity, const std::string& message) const
{
    m_pImpl->LogMsg(verbosity, message.c_str());
}

void Log::LogMsg(LogVerbosity verbosity, const char* message) const
{
    m_pImpl->LogMsg(verbosity, message);
}