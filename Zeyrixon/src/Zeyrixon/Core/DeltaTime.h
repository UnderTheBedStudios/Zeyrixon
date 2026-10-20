#pragma once

namespace Zeyrixon
{
    class DeltaTime
    {
    public:
        DeltaTime(float time = 0.f)
            : m_Time(time)
        {
        }

        float GetSeconds() const { return m_Time; }
        float GetMilliseconds() const { return m_Time * 1000.f; }

        operator float() const { return GetSeconds(); }

    private:
        float m_Time;
    };
}
