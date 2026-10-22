#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Platform/OpenGL/OpenGLTexture.h>

#include <memory>
#include <string>
#include <vector>

namespace Zeyrixon
{
    /* Loads each (file, color space) pair once. Color maps such as albedo are sRGB; data maps
       such as normal, roughness, and metallic are not. Returns null if the file can't be loaded
       (the failure is logged once and remembered). */
    class Z_API TextureLibrary
    {
    public:
        static std::shared_ptr<OpenGLTexture2D> Get(const std::string& path, bool srgb);

        /* Gives the library the still-encoded bytes (PNG/JPG) of a texture that has no file of its own,
           such as an image packed inside a .glb. After this, Get(key, ...) decodes and uploads it on
           first use. glTF images use keys like "gltf:Models/Robot.glb#3". */
        static void RegisterEncoded(const std::string& key, std::vector<unsigned char> bytes);
    };
}
