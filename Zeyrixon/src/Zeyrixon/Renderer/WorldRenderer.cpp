#include <pch.h>

#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/Renderer/TextureLibrary.h>
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

            out vec3 v_WorldPos;
            out vec3 v_Normal;
            out vec2 v_TexCoord;

            void main()
            {
                vec4 world = u_Model * vec4(a_Position, 1.0);
                v_WorldPos = world.xyz;
                v_Normal = u_NormalMatrix * a_Normal;
                v_TexCoord = a_TexCoord;
                gl_Position = u_ViewProjection * world;
            }
        )";

        const char* k_FragmentSource = R"(
            #version 450 core
            in vec3 v_WorldPos;
            in vec3 v_Normal;
            in vec2 v_TexCoord;

            uniform vec3 u_CameraPosition;
            uniform vec3 u_LightDirection;   // direction towards the light
            uniform vec3 u_LightColor;

            uniform vec3 u_Albedo;
            uniform float u_Metallic;
            uniform float u_Roughness;
            uniform float u_AO;

            uniform sampler2D u_AlbedoMap;
            uniform sampler2D u_NormalMap;
            uniform sampler2D u_RoughnessMap;
            uniform sampler2D u_MetallicMap;
            uniform sampler2D u_AOMap;

            uniform int u_HasAlbedoMap;
            uniform int u_HasNormalMap;
            uniform int u_HasRoughnessMap;
            uniform int u_HasMetallicMap;
            uniform int u_HasAOMap;

            out vec4 o_Color;

            const float PI = 3.14159265359;

            // Tangent-space normal map -> world space. The tangent frame is built from screen-space
            // derivatives, so meshes don't need per-vertex tangents. Assumes OpenGL-style
            // normal maps (+Y up), which is what Blender exports.
            vec3 GetNormal()
            {
                vec3 N = normalize(v_Normal);
                if (u_HasNormalMap == 0)
                    return N;

                vec3 tangentNormal = texture(u_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;

                vec3 Q1 = dFdx(v_WorldPos);
                vec3 Q2 = dFdy(v_WorldPos);
                vec2 st1 = dFdx(v_TexCoord);
                vec2 st2 = dFdy(v_TexCoord);

                vec3 T = Q1 * st2.t - Q2 * st1.t;
                if (dot(T, T) < 1e-12)
                    return N;   // degenerate UVs

                T = normalize(T);
                vec3 B = -normalize(cross(N, T));
                return normalize(mat3(T, B, N) * tangentNormal);
            }

            float DistributionGGX(vec3 N, vec3 H, float roughness)
            {
                float a = roughness * roughness;
                float a2 = a * a;
                float NdotH = max(dot(N, H), 0.0);
                float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
                return a2 / (PI * denom * denom);
            }

            float GeometrySchlickGGX(float NdotV, float roughness)
            {
                float r = roughness + 1.0;
                float k = (r * r) / 8.0;
                return NdotV / (NdotV * (1.0 - k) + k);
            }

            float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
            {
                return GeometrySchlickGGX(max(dot(N, V), 0.0), roughness)
                     * GeometrySchlickGGX(max(dot(N, L), 0.0), roughness);
            }

            vec3 FresnelSchlick(float cosTheta, vec3 F0)
            {
                return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
            }

            void main()
            {
                vec3 albedo = u_Albedo;
                if (u_HasAlbedoMap != 0)
                    albedo *= texture(u_AlbedoMap, v_TexCoord).rgb;

                float metallic  = (u_HasMetallicMap  != 0) ? texture(u_MetallicMap,  v_TexCoord).r : u_Metallic;
                float roughness = (u_HasRoughnessMap != 0) ? texture(u_RoughnessMap, v_TexCoord).r : u_Roughness;
                float ao        = (u_HasAOMap        != 0) ? texture(u_AOMap,        v_TexCoord).r : u_AO;
                roughness = clamp(roughness, 0.04, 1.0);   // a perfectly smooth surface gives a degenerate highlight

                vec3 N = GetNormal();
                vec3 V = normalize(u_CameraPosition - v_WorldPos);
                vec3 L = normalize(u_LightDirection);
                vec3 H = normalize(V + L);

                vec3 F0 = mix(vec3(0.04), albedo, metallic);

                // Cook-Torrance specular + Lambert diffuse for the directional light.
                float NDF = DistributionGGX(N, H, roughness);
                float G = GeometrySmith(N, V, L, roughness);
                vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 specular = (NDF * G * F)
                              / (4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001);
                vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

                float NdotL = max(dot(N, L), 0.0);
                vec3 Lo = (kD * albedo / PI + specular) * u_LightColor * NdotL;

                // Hemisphere ambient: a stand-in until image-based lighting exists.
                vec3 hemisphere = mix(vec3(0.15, 0.13, 0.12), vec3(0.45, 0.55, 0.70), N.y * 0.5 + 0.5);
                vec3 ambient = ((1.0 - metallic) * albedo * hemisphere
                              + F0 * hemisphere * (1.0 - roughness)) * ao;

                vec3 color = ambient + Lo;

                color = color / (color + vec3(1.0));   // Reinhard tone mapping
                color = pow(color, vec3(1.0 / 2.2));   // the framebuffer is not sRGB
                o_Color = vec4(color, 1.0);
            }
        )";

        // Created on first use (the GL context exists by then) and deliberately never freed,
        // for the same reason as the mesh cache.
        OpenGLShader& GetShader()
        {
            static OpenGLShader* shader = new OpenGLShader("PBR", k_VertexSource, k_FragmentSource);
            return *shader;
        }

        // What an entity with a mesh but no material component is drawn with.
        const MaterialComponent& DefaultMaterial()
        {
            static const MaterialComponent material = []
            {
                MaterialComponent m;
                m.Albedo = glm::vec3(0.8f);
                return m;
            }();
            return material;
        }

        // Binds a texture to a slot and tells the shader whether it is there.
        void BindMap(OpenGLShader& shader, const std::string& path, bool srgb, uint32_t slot, const char* hasUniform)
        {
            std::shared_ptr<OpenGLTexture2D> texture;
            if (!path.empty())
                texture = TextureLibrary::Get(path, srgb);

            shader.SetInt(hasUniform, texture ? 1 : 0);
            if (texture)
                texture->Bind(slot);
        }
    }

    void WorldRenderer::RenderWorld(World& world, const glm::mat4& view, const glm::mat4& projection,
                                    const glm::vec3& cameraPosition)
    {
        OpenGLShader& shader = GetShader();
        shader.Bind();

        shader.SetMat4("u_ViewProjection", projection * view);
        shader.SetFloat3("u_CameraPosition", cameraPosition);
        shader.SetFloat3("u_LightDirection", glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f)));
        shader.SetFloat3("u_LightColor", glm::vec3(3.0f, 2.9f, 2.7f));

        shader.SetInt("u_AlbedoMap", 0);
        shader.SetInt("u_NormalMap", 1);
        shader.SetInt("u_RoughnessMap", 2);
        shader.SetInt("u_MetallicMap", 3);
        shader.SetInt("u_AOMap", 4);

        auto& registry = world.GetRegistry();
        auto entities = registry.view<TransformComponent, MeshComponent>();
        for (auto [entity, transform, mesh] : entities.each())
        {
            std::shared_ptr<Mesh> meshData = MeshLibrary::Get(mesh.MeshPath);
            if (!meshData)
                continue;

            const MaterialComponent& material = registry.all_of<MaterialComponent>(entity)
                ? registry.get<MaterialComponent>(entity)
                : DefaultMaterial();

            const glm::mat4 model = transform.GetMatrix();
            shader.SetMat4("u_Model", model);
            shader.SetMat3("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));

            shader.SetFloat3("u_Albedo", material.Albedo);
            shader.SetFloat("u_Metallic", material.Metallic);
            shader.SetFloat("u_Roughness", material.Roughness);
            shader.SetFloat("u_AO", material.AO);

            BindMap(shader, material.AlbedoMap,    true,  0, "u_HasAlbedoMap");
            BindMap(shader, material.NormalMap,    false, 1, "u_HasNormalMap");
            BindMap(shader, material.RoughnessMap, false, 2, "u_HasRoughnessMap");
            BindMap(shader, material.MetallicMap,  false, 3, "u_HasMetallicMap");
            BindMap(shader, material.AOMap,        false, 4, "u_HasAOMap");

            OpenGLRender::DrawIndexed(meshData->GetVertexArray());
        }
    }
}
