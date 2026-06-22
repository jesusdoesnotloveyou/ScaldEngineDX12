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

struct Log::Impl
{
    Impl()
    {
        const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_Logger = std::make_unique<spdlog::logger>("win32", consoleSink);
    }

    std::unique_ptr<spdlog::logger> m_Logger;
};

Log::Log() : m_Impl(std::make_unique<Impl>()) {}

// For using unique_ptr for PIMPL 
Log::~Log() = default;  

void Log::LogMsg(LogVerbosity verbosity, const char* message) const
{
    if (verbosity == LogVerbosity::NoLogging) return;

    const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    const auto consoleLogger = std::make_unique<spdlog::logger>("win32", consoleSink);

    if (const auto spdLevel = c_verbosityMap.find(verbosity); spdLevel != c_verbosityMap.end())
    {
        consoleLogger->log(spdLevel->second, message);
    }
}

void Log::LogMsg(LogVerbosity verbosity, const std::string& message) const
{
    LogMsg(verbosity, message.c_str());
}