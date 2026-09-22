#pragma once

#include <Zeyrixon/Core/Core.h>

#include <cstdint>

namespace Zeyrixon
{
    class Z_API OpenGLUniformBuffer
    {
    public:
        OpenGLUniformBuffer(uint32_t size, uint32_t binding);
        ~OpenGLUniformBuffer();

        void SetData(const void* data, uint32_t size, uint32_t offset = 0);
        
    private:
        uint32_t m_RenderID = 0;
    };
}