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

        // Encoded images that live inside another file (see RegisterEncoded).
        std::unordered_map<std::string, std::vector<unsigned char>>& Embedded()
        {
            static auto* embedded = new std::unordered_map<std::string, std::vector<unsigned char>>();
            return *embedded;
        }
    }

    void TextureLibrary::RegisterEncoded(const std::string& key, std::vector<unsigned char> bytes)
    {
        Embedded()[key] = std::move(bytes);

        // If the key was asked for earlier, the answer cached then (a failure) is out of date.
        Cache().erase(key + "|srgb");
        Cache().erase(key + "|linear");
    }

    std::shared_ptr<OpenGLTexture2D> TextureLibrary::Get(const std::string& path, bool srgb)
    {
        auto& cache = Cache();
        const std::string key = path + (srgb ? "|srgb" : "|linear");

        auto found = cache.find(key);
        if (found != cache.end())
            return found->second;

        std::shared_ptr<OpenGLTexture2D> texture;
        if (path.rfind("gltf:", 0) == 0)
        {
            // Not a file on disk. If the model that owns it hasn't been loaded yet, answer "not
            // there" without caching that, so asking again after it has loaded works.
            auto embedded = Embedded().find(path);
            if (embedded == Embedded().end())
                return nullptr;

            texture = std::make_shared<OpenGLTexture2D>(embedded->second.data(), embedded->second.size(), srgb);
        }
        else
        {
            texture = std::make_shared<OpenGLTexture2D>(path, srgb);
        }

        if (!texture->IsLoaded())
            texture = nullptr;

        cache[key] = texture;
        return texture;
    }
}
