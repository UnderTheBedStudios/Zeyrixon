#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Platform/OpenGL/OpenGLTexture.h>

#include <memory>
#include <string>

namespace Zeyrixon
{
    /* Loads each (file, color space) pair once. Color maps such as albedo are sRGB; data maps
       such as normal, roughness, and metallic are not. Returns null if the file can't be loaded
       (the failure is logged once and remembered). */
    class Z_API TextureLibrary
    {
    public:
        static std::shared_ptr<OpenGLTexture2D> Get(const std::string& path, bool srgb);
    };
}
