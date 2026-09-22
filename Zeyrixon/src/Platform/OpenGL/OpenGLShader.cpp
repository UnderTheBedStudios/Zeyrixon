#include <pch.h>

#include <Platform/OpenGL/OpenGLShader.h>
#include <Zeyrixon/Core/Log.h>

#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <fstream>

namespace Zeyrixon
{
    static GLenum ShaderTypeFromString(const std::string& type)
    {
        if (type == "vertex")   return GL_VERTEX_SHADER;
        if (type == "fragment") return GL_FRAGMENT_SHADER;

        Z_CORE_CRITICAL("Unknown shader type '{0}'!", type);
        return 0;
    }

    OpenGLShader::OpenGLShader(const std::string& filepath)
    {
        std::string source = ReadFile(filepath);
        auto shaderSources = PreProcess(source);
        Compile(shaderSources);

        // Derive a display name from the filepath, e.g. "assets/shaders/Basic.glsl" -> "Basic"
        std::filesystem::path path = filepath;
        m_Name = path.stem().string();
    }

    OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
        : m_Name(name)
    {
        std::unordered_map<uint32_t, std::string> shaderSources;
        shaderSources[GL_VERTEX_SHADER] = vertexSrc;
        shaderSources[GL_FRAGMENT_SHADER] = fragmentSrc;
        Compile(shaderSources);
    }

    OpenGLShader::~OpenGLShader()
    {
        glDeleteProgram(m_RendererID);
    }

    std::string OpenGLShader::ReadFile(const std::string& filepath)
    {
        std::ifstream in(filepath, std::ios::in | std::ios::binary);
        if (!in)
        {
            Z_CORE_CRITICAL("Could not open shader file '{0}'!", filepath);
            return std::string();
        }

        std::string result;
        in.seekg(0, std::ios::end);
        result.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&result[0], result.size());
        in.close();

        return result;
    }

    std::unordered_map<uint32_t, std::string> OpenGLShader::PreProcess(const std::string& source)
    {
        std::unordered_map<uint32_t, std::string> shaderSources;

        const char* typeToken = "#type";
        size_t typeTokenLength = strlen(typeToken);
        size_t pos = source.find(typeToken, 0);

        while (pos != std::string::npos)
        {
            size_t eol = source.find_first_of("\r\n", pos);
            if (eol == std::string::npos)
            {
                Z_CORE_CRITICAL("Syntax error in shader: expected a newline after '#type' declaration!");
                break;
            }

            size_t begin = pos + typeTokenLength + 1;
            std::string type = source.substr(begin, eol - begin);
            if (!ShaderTypeFromString(type))
            {
                Z_CORE_CRITICAL("Invalid shader type specified: '{0}'!", type);
                break;
            }

            size_t nextLinePos = source.find_first_not_of("\r\n", eol);
            pos = source.find(typeToken, nextLinePos);
            shaderSources[ShaderTypeFromString(type)] = source.substr(
                nextLinePos,
                pos - (nextLinePos == std::string::npos ? source.size() : nextLinePos));
        }

        return shaderSources;
    }

    void OpenGLShader::Compile(const std::unordered_map<uint32_t, std::string>& shaderSources)
    {
        GLuint program = glCreateProgram();

        std::vector<GLenum> glShaderIDs;
        glShaderIDs.reserve(shaderSources.size());

        for (auto& kv : shaderSources)
        {
            GLenum type = kv.first;
            const std::string& source = kv.second;

            GLuint shader = glCreateShader(type);

            const GLchar* sourceCStr = source.c_str();
            glShaderSource(shader, 1, &sourceCStr, nullptr);
            glCompileShader(shader);

            GLint isCompiled = 0;
            glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
            if (isCompiled == GL_FALSE)
            {
                GLint maxLength = 0;
                glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

                std::vector<GLchar> infoLog(maxLength);
                glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());

                glDeleteShader(shader);

                Z_CORE_CRITICAL("Shader '{0}' compilation failed:\n{1}", m_Name, infoLog.data());
                break;
            }

            glAttachShader(program, shader);
            glShaderIDs.push_back(shader);
        }

        glLinkProgram(program);

        GLint isLinked = 0;
        glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
        if (isLinked == GL_FALSE)
        {
            GLint maxLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

            std::vector<GLchar> infoLog(maxLength);
            glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());

            glDeleteProgram(program);
            for (auto id : glShaderIDs)
                glDeleteShader(id);

            Z_CORE_CRITICAL("Shader '{0}' link failed:\n{1}", m_Name, infoLog.data());
            return;
        }

        for (auto id : glShaderIDs)
        {
            glDetachShader(program, id);
            glDeleteShader(id);
        }

        m_RendererID = program;
    }

    void OpenGLShader::Bind() const
    {
        glUseProgram(m_RendererID);
    }

    void OpenGLShader::Unbind() const
    {
        glUseProgram(0);
    }

    int OpenGLShader::GetUniformLocation(const std::string& name)
    {
        auto it = m_UniformLocationCache.find(name);
        if (it != m_UniformLocationCache.end())
            return it->second;

        int location = glGetUniformLocation(m_RendererID, name.c_str());
        if (location == -1)
            Z_CORE_WARN("Uniform '{0}' not found in shader '{1}' (or optimized out for being unused)", name, m_Name);

        m_UniformLocationCache[name] = location;
        return location;
    }

    void OpenGLShader::SetInt(const std::string& name, int value)
    {
        glUniform1i(GetUniformLocation(name), value);
    }

    void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count)
    {
        glUniform1iv(GetUniformLocation(name), (GLsizei)count, values);
    }

    void OpenGLShader::SetFloat(const std::string& name, float value)
    {
        glUniform1f(GetUniformLocation(name), value);
    }

    void OpenGLShader::SetFloat2(const std::string& name, const glm::vec2& value)
    {
        glUniform2f(GetUniformLocation(name), value.x, value.y);
    }

    void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value)
    {
        glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
    }

    void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value)
    {
        glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
    }

    void OpenGLShader::SetMat3(const std::string& name, const glm::mat3& value)
    {
        glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }

    void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value)
    {
        glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
    }
}