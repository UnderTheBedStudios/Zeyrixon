#pragma once

#include <memory>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/ostr.h>

#include <Zeyrixon/Core/Core.h>

namespace Zeyrixon
{
    class Z_API Log
    {
    public:
        /**
         * Sets the logging pattern to the correct format.
         *
         * Example:
         *
         * [ZEYRIXON]: Hello, World!
         * [GAME]: Hello to you too!
         *
         * It also sets everything to the correct color.
         */
        static void Init();

        /* It can get the core(Engine) logger */
        inline static std::shared_ptr<spdlog::logger>& GetCoreLogger() { return m_CoreLogger; }
        /* It can get the game(Client) logger */
        inline static std::shared_ptr<spdlog::logger>& GetClientLogger() { return m_ClientLogger; }
        /* It can get the editor logger */
        inline static std::shared_ptr<spdlog::logger>& GetEditorLogger() { return m_EditorLogger; }
    private:
        static std::shared_ptr<spdlog::logger> m_CoreLogger;
        static std::shared_ptr<spdlog::logger> m_ClientLogger;
        static std::shared_ptr<spdlog::logger> m_EditorLogger;
    };
}

//------------------------------ Core Log Macros ------------------------------

/* Logs an error for the Engine */
#define Z_CORE_ERROR(...)       ::Zeyrixon::Log::GetCoreLogger()->error(__VA_ARGS__)
/* Logs a warnning for the Engine */
#define Z_CORE_WARN(...)        ::Zeyrixon::Log::GetCoreLogger()->warn(__VA_ARGS__)
/* Logs information for the Engine */
#define Z_CORE_INFO(...)        ::Zeyrixon::Log::GetCoreLogger()->info(__VA_ARGS__)
/* Logs random bs for the Engine */
#define Z_CORE_TRACE(...)       ::Zeyrixon::Log::GetCoreLogger()->trace(__VA_ARGS__)
/* Logs a critical error for the Engine */
#define Z_CORE_CRITICAL(...)    ::Zeyrixon::Log::GetCoreLogger()->critical(__VA_ARGS__)

//------------------------------ Editor Log Macros ------------------------------

/* Logs an error for the Editor */
#define Z_EDITOR_ERROR(...)     ::Zeyrixon::Log::GetEditorLogger()->error(__VA_ARGS__)
/* Logs a warnning for the Editor */
#define Z_EDITOR_WARN(...)      ::Zeyrixon::Log::GetEditorLogger()->warn(__VA_ARGS__)
/* Logs information for the Editor */
#define Z_EDITOR_INFO(...)      ::Zeyrixon::Log::GetEditorLogger()->info(__VA_ARGS__)
/* Logs random bs for the Editor */
#define Z_EDITOR_TRACE(...)     ::Zeyrixon::Log::GetEditorLogger()->trace(__VA_ARGS__)
/* Logs a critical error for the Editor */
#define Z_EDITOR_CRITICAL(...)  ::Zeyrixon::Log::GetEditorLogger()->critical(__VA_ARGS__)

//------------------------------ Client Log Macros ------------------------------

/* Logs an error for the Client */
#define Z_ERROR(...)     ::Zeyrixon::Log::GetClientLogger()->error(__VA_ARGS__)
/* Logs a warnning for the Client */
#define Z_WARN(...)      ::Zeyrixon::Log::GetClientLogger()->warn(__VA_ARGS__)
/* Logs information for the Client */
#define Z_INFO(...)      ::Zeyrixon::Log::GetClientLogger()->info(__VA_ARGS__)
/* Logs random bs for the Client */
#define Z_TRACE(...)     ::Zeyrixon::Log::GetClientLogger()->trace(__VA_ARGS__)
/* Logs a critical error for the Client */
#define Z_CRITICAL(...)  ::Zeyrixon::Log::GetClientLogger()->critical(__VA_ARGS__)