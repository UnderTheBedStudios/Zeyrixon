#include <pch.h>
#include <Zeyrixon/Log.h>

#include <spdlog/sinks/stdout_color_sinks.h>

namespace Zeyrixon
{
    std::shared_ptr<spdlog::logger> Log::m_ClientLogger;
    std::shared_ptr<spdlog::logger> Log::m_CoreLogger;

    void Log::Init()
    {
        spdlog::set_pattern("%^[%T] %n: %v%$");

        m_CoreLogger = spdlog::stdout_color_mt("ZEYRIXON");
        m_CoreLogger->set_level(spdlog::level::trace);

        m_ClientLogger = spdlog::stdout_color_mt("GAME");
        m_ClientLogger->set_level(spdlog::level::trace);
    }
}