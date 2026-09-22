#pragma once

#include <Zeyrixon/Core/Core.h>

#include <glm/glm.hpp>

#include <cstdint>
#include <string>
#include <unordered_map>

namespace Zeyrixon
{
    class Z_API OpenGLShader
    {
    public:
        // Loads a single file containing both stages, split by "#type vertex" / "#type fragment" markers
        OpenGLShader(const std::string& filepath);
        // Builds directly from source strings, with an explicit name (no filepath to derive one from)
        OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
        ~OpenGLShader();

        void Bind() const;
        void Unbind() const;

        void SetInt(const std::string& name, int value);
        void SetIntArray(const std::string& name, int* values, uint32_t count);
        void SetFloat(const std::string& name, float value);
        void SetFloat2(const std::string& name, const glm::vec2& value);
        void SetFloat3(const std::string& name, const glm::vec3& value);
        void SetFloat4(const std::string& name, const glm::vec4& value);
        void SetMat3(const std::string& name, const glm::mat3& value);
        void SetMat4(const std::string& name, const glm::mat4& value);

        const std::string& GetName() const { return m_Name; }

    private:
        std::string ReadFile(const std::string& filepath);
        std::unordered_map<uint32_t, std::string> PreProcess(const std::string& source);
        void Compile(const std::unordered_map<uint32_t, std::string>& shaderSources);

        int GetUniformLocation(const std::string& name);

        uint32_t m_RendererID = 0;
        std::string m_Name;

        std::unordered_map<std::string, int> m_UniformLocationCache;
    };
}