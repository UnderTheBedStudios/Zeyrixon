#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <Zeyrixon/Core.h>

namespace Zeyrixon
{
    class Z_API Log
    {
    public:
        static void Init();

        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return m_CoreLogger; }
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() {return m_ClientLogger; }
    private:
        static std::shared_ptr<spdlog::logger> m_CoreLogger;
        static std::shared_ptr<spdlog::logger> m_ClientLogger;
    };
}

//------------------------------ Core Log Macros ------------------------------

#define Z_CORE_ERROR(...)       ::Zeyrixon::Log::GetCoreLogger()->error(__VA__ARGS__)
#define Z_CORE_WARN(...)        ::Zeyrixon::Log::GetCoreLogger()->warn(__VA_ARGS__)
#define Z_CORE_INFO(...)        ::Zeyrixon::Log::GetCoreLogger()->info(__VA_ARGS__)
#define Z_CORE_TRACE(...)       ::Zeyrixon::Log::GetCoreLogger()->trace(__VA_ARGS__)
#define Z_CORE_CRITICAL(...)    ::Zeyrixon::Log::GetCoreLogger()->critical(__VA_ARGS__)

//------------------------------ Client Log Macros ------------------------------

#define Z_ERROR(...)     ::Zeyrixon::Log::GetClientLogger()->error(__VA__ARGS__)
#define Z_WARN(...)      ::Zeyrixon::Log::GetClientLogger()->warn(__VA_ARGS__)
#define Z_INFO(...)      ::Zeyrixon::Log::GetClientLogger()->info(__VA_ARGS__)
#define Z_TRACE(...)     ::Zeyrixon::Log::GetClientLogger()->trace(__VA_ARGS__)
#define Z_CRITICAL(...)  ::Zeyrixon::Log::GetClientLogger()->critical(__VA_ARGS__)