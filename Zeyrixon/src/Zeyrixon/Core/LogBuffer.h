#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/details/os.h>
#include <spdlog/sinks/base_sink.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace Zeyrixon
{
    struct LogEntry
    {
        spdlog::level::level_enum Level = spdlog::level::info;
        std::string Time;      // HH:MM:SS
        std::string Logger;    // ZEYRIXON, EDITOR or GAME
        std::string Message;
    };

    // A spdlog sink that keeps the most recent messages in memory so the editor can show them.
    class LogBuffer : public spdlog::sinks::base_sink<std::mutex>
    {
    public:
        explicit LogBuffer(size_t capacity = 2000) : m_Capacity(capacity) {}

        /* Changes whenever something is logged or the buffer is cleared, so the UI can skip
           copying when nothing changed. */
        uint64_t GetVersion() const { return m_Version.load(); }
        void CopyEntries(std::vector<LogEntry>& out)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            out.assign(m_Entries.begin(), m_Entries.end());
        }

        void Clear()
        {
            std::lock_guard<std::mutex> lock(mutex_);
            m_Entries.clear();
            ++m_Version;
        }

    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            LogEntry entry;
            entry.Level = msg.level;
            entry.Logger.assign(msg.logger_name.data(), msg.logger_name.size());
            entry.Message.assign(msg.payload.data(), msg.payload.size());

            const std::time_t seconds = std::chrono::system_clock::to_time_t(msg.time);
            const std::tm local = spdlog::details::os::localtime(seconds);
            char timeText[16];
            std::strftime(timeText, sizeof(timeText), "%H:%M:%S", &local);
            entry.Time = timeText;

            m_Entries.push_back(std::move(entry));
            while (m_Entries.size() > m_Capacity)
                m_Entries.pop_front();

            ++m_Version;
        }

        void flush_() override {}

    private:
        size_t m_Capacity;
        std::deque<LogEntry> m_Entries;
        std::atomic<uint64_t> m_Version{ 0 };
    };
}
