#include <pch.h>

#include <Zeyrixon/Renderer/TextureLibrary.h>

#include <unordered_map>

namespace Zeyrixon
{
    namespace
    {
        /* Deliberately never destroyed, for the same reason as the mesh cache: freeing GL
           textures during static teardown would happen after GL context is gone. */
        std::unordered_map<std::string, std::shared_ptr<OpenGLTexture2D>>& Cache()
        {
            static auto* cache = new std::unordered_map<std::string, std::shared_ptr<OpenGLTexture2D>>();
            return *cache;
        }
    }

    std::shared_ptr<OpenGLTexture2D> TextureLibrary::Get(const std::string& path, bool srgb)
    {
        auto& cache = Cache();
        const std::string key = path + (srgb ? "|srgb" : "|linear");

        auto found = cache.find(key);
        if (found != cache.end())
            return found->second;

        auto texture = std::make_shared<OpenGLTexture2D>(path, srgb);
        if (!texture->IsLoaded())
            texture = nullptr;

        cache[key] = texture;
        return texture;
    }
}
