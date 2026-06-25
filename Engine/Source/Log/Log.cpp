#include "Log.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

#include <memory>
#include <unordered_map>
#include <format>

using namespace Scald;

namespace
{
    const std::unordered_map<LogVerbosity, spdlog::level::level_enum> kVerbosityMap = 
    {
        {LogVerbosity::NoLogging, spdlog::level::off},
        {LogVerbosity::Display, spdlog::level::info},
        {LogVerbosity::Warning, spdlog::level::warn},
        {LogVerbosity::Error, spdlog::level::err},
        {LogVerbosity::Fatal, spdlog::level::critical}
    };

    constexpr const char* kLogPattern = "[%H:%M:%S.%e] [%^%l%$] %v";
    }

// pImpl
struct Log::Impl
{
    Impl()
    {
        const auto consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        m_logger = std::make_unique<spdlog::logger>("Win32Console", consoleSink);
        m_logger->set_pattern(kLogPattern);
    }

    void LogMsg(LogVerbosity verbosity, const std::string& message) const
    {
        if (verbosity == LogVerbosity::NoLogging) return;

        const auto spdLevelPairIt = kVerbosityMap.find(verbosity);
        if (spdLevelPairIt == kVerbosityMap.end()) return;
        
        m_logger->log(spdLevelPairIt->second, message);

        if (verbosity == LogVerbosity::Fatal)
        {
            PLATFORM_BREAK();
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
void Log::LogMsg(const LogCategory& category, LogVerbosity verbosity, const std::string& message) const
{
    m_pImpl->LogMsg(verbosity, std::format("[{}] {}", category.GetName(), message));
}

void Log::LogMsg(const LogCategory& category, LogVerbosity verbosity, const char* message) const
{
    m_pImpl->LogMsg(verbosity, std::format("[{}] {}", category.GetName(), message));
}