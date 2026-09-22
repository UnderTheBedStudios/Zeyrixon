#pragma once

#include <Zeyrixon/Core/Core.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>

namespace Zeyrixon
{
    class Z_API OpenGLTexture2D
    {
    public:
        OpenGLTexture2D(const std::string& path);
        OpenGLTexture2D(uint32_t width, uint32_t height);
        OpenGLTexture2D(glm::vec2 size);

        ~OpenGLTexture2D();

        void SetTextureSize(uint32_t width, uint32_t height);

        uint32_t GetWidth() const { return m_Width; }
        uint32_t GetHeight() const { return m_Height; }

        void SetData(void* data, uint32_t size);

        void Bind(uint32_t slot = 0) const;

        bool IsLoaded() const { return m_IsLoaded; }

        const std::string& GetPath() const { return m_Path; }
        
    private:
        std::string m_Path;
        bool m_IsLoaded = false;

        uint32_t m_Width = 0;
        uint32_t m_Height = 0;
        uint32_t m_RendererID = 0;

        uint32_t m_InternalFormat = 0;
        uint32_t m_DataFormat = 0;
    };
}