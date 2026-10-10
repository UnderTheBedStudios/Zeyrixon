#include <pch.h>

#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/World/Components.h>

#include <Platform/OpenGL/OpenGLRender.h>
#include <Platform/OpenGL/OpenGLShader.h>

namespace Zeyrixon
{
    namespace
    {
        const char* k_VertexSource = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec3 a_Normal;
            layout(location = 2) in vec2 a_TexCoord;

            uniform mat4 u_ViewProjection;
            uniform mat4 u_Model;
            uniform mat3 u_NormalMatrix;

            out vec3 v_Normal;

            void main()
            {
                v_Normal = normalize(u_NormalMatrix * a_Normal);
                gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
            }
        )";

        const char* k_FragmentSource = R"(
            #version 450 core
            in vec3 v_Normal;

            uniform vec3 u_Color;

            out vec4 o_Color;

            void main()
            {
                vec3 lightDirection = normalize(vec3(0.4, 0.8, 0.6));
                float diffuse = max(dot(normalize(v_Normal), lightDirection), 0.0);
                o_Color = vec4(u_Color * (0.25 + 0.75 * diffuse), 1.0);
            }
        )";

        // Created on first use (the GL context exists by then) and deliberately never freed,
        // for the same reason as the mesh cache.
        OpenGLShader& GetShader()
        {
            static OpenGLShader* shader = new OpenGLShader("Mesh", k_VertexSource, k_FragmentSource);
            return *shader;
        }
    }

    void WorldRenderer::RenderWorld(World& world, const glm::mat4& view, const glm::mat4& projection)
    {
        OpenGLShader& shader = GetShader();
        shader.Bind();
        shader.SetMat4("u_ViewProjection", projection * view);

        auto entities = world.GetRegistry().view<TransformComponent, MeshComponent>();
        for (auto [entity, transform, mesh] : entities.each())
        {
            std::shared_ptr<Mesh> meshData = MeshLibrary::Get(mesh.MeshPath);
            if (!meshData)
                continue;

            const glm::mat4 model = transform.GetMatrix();
            shader.SetMat4("u_Model", model);
            shader.SetMat3("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));
            shader.SetFloat3("u_Color", mesh.Color);

            OpenGLRender::DrawIndexed(meshData->GetVertexArray());
        }
    }
}
