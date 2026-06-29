#include "Log.h"

#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

#include <filesystem>
#include <chrono>
#include <memory>
#include <unordered_map>
#include <format>

using namespace Scald;

namespace fs = std::filesystem;

namespace
{
    const std::unordered_map<LogVerbosity, spdlog::level::level_enum> kVerbosityMap = 
    {
        {LogVerbosity::NoLogging, spdlog::level::off},
        {LogVerbosity::Display, spdlog::level::info},
        {LogVerbosity::Warning, spdlog::level::warn},
        {LogVerbosity::Error, spdlog::level::err},
        {LogVerbosity::Log, spdlog::level::info},
        {LogVerbosity::Fatal, spdlog::level::critical}
    };

    // Log pattern: [HH:MM:SS.milliseconds] [LogLevel] Message
    constexpr const char* kLogPattern = "[%H:%M:%S.%e] [%^%l%$] %v";

    const fs::path kLogDirectory = "logs";
    constexpr const char* kLogFilePrefix = "Scald";
    constexpr const char* kLogFileExtension = "txt";
    constexpr const char* kTimestampFormat = "{:%Y.%m.%d-%H.%M.%S}";
    }

// pImpl
struct Log::Impl
{
    Impl()
    {
        using namespace spdlog;
        const auto consoleSink = std::make_shared<sinks::stdout_color_sink_mt>();
        m_consoleLogger = std::make_unique<logger>("Win32Console", consoleSink);
        m_consoleLogger->set_pattern(kLogPattern);

        const auto fileSink = std::make_shared<sinks::basic_file_sink_mt>(MakeLogFile().string(), true);
        m_fileLogger = std::make_unique<logger>("FileLogger", fileSink);
        m_fileLogger->set_pattern(kLogPattern);
    }

    void LogMsg(LogVerbosity verbosity, const std::string& message) const
    {
        if (verbosity == LogVerbosity::NoLogging) return;

        const auto spdLevelPairIt = kVerbosityMap.find(verbosity);
        if (spdLevelPairIt == kVerbosityMap.end()) return;
        
        const auto spdLevel = spdLevelPairIt->second;
        // Avoid writing to console logger for LogVerbosity::Log
        if (verbosity != LogVerbosity::Log && m_consoleLogger->should_log(spdLevel))
        {
            m_consoleLogger->log(spdLevel, message);
        }
        // But write to file logger for all verbosity levels
        if (m_fileLogger->should_log(spdLevel))
        {
            m_fileLogger->log(spdLevel, message);
        }

        if (verbosity == LogVerbosity::Fatal)
        {
            // UE-like
            PLATFORM_BREAK();
        }
    }

private:
    fs::path MakeLogFile() const
    {
        fs::create_directory(kLogDirectory);
        const auto now = std::chrono::system_clock::now();
        const auto nowSeconds = std::chrono::floor<std::chrono::seconds>(now);
        const std::string timestamp = std::format(kTimestampFormat, nowSeconds);
        const std::string logName = std::format("{}-{}.{}", kLogFilePrefix, timestamp, kLogFileExtension);
        return kLogDirectory / logName;
    }

private:
    std::unique_ptr<spdlog::logger> m_consoleLogger;
    std::unique_ptr<spdlog::logger> m_fileLogger;

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