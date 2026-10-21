#include <pch.h>

#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/Renderer/TextureLibrary.h>
#include <Zeyrixon/World/Components.h>

#include <Platform/OpenGL/OpenGLRender.h>
#include <Platform/OpenGL/OpenGLShader.h>

#include <glad/glad.h>

#include <algorithm>
#include <vector>

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

        // What a submesh with no material slot is drawn with.
        const MaterialSlot& DefaultMaterial()
        {
            static const MaterialSlot material = []
            {
                MaterialSlot m;
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

        // Uploads one material's values and binds its maps, ready for the next draw call.
        void ApplyMaterial(OpenGLShader& shader, const MaterialSlot& material)
        {
            shader.SetFloat3("u_Albedo", material.Albedo);
            shader.SetFloat("u_Metallic", material.Metallic);
            shader.SetFloat("u_Roughness", material.Roughness);
            shader.SetFloat("u_AO", material.AO);

            BindMap(shader, material.AlbedoMap,    true,  0, "u_HasAlbedoMap");
            BindMap(shader, material.NormalMap,    false, 1, "u_HasNormalMap");
            BindMap(shader, material.RoughnessMap, false, 2, "u_HasRoughnessMap");
            BindMap(shader, material.MetallicMap,  false, 3, "u_HasMetallicMap");
            BindMap(shader, material.AOMap,        false, 4, "u_HasAOMap");
        }
    }

    const char* k_GridVertexSource = R"(
        #version 450 core
        layout(location = 0) in vec3 a_Position;   // unit quad, x and z in [-1, 1]

        uniform mat4 u_ViewProjection;
        uniform vec3 u_Center;
        uniform float u_Extent;
        uniform int u_XYPlane;

        out vec3 v_WorldPos;

        void main()
        {
            vec3 offset = (u_XYPlane == 0)
                ? vec3(a_Position.x, 0.0, a_Position.z)
                : vec3(a_Position.x, a_Position.z, 0.0);

            vec3 world = u_Center + offset * u_Extent;
            v_WorldPos = world;
            gl_Position = u_ViewProjection * vec4(world, 1.0);
        }
    )";

    const char* k_GridFragmentSource = R"(
        #version 450 core
        in vec3 v_WorldPos;

        uniform vec3 u_Center;
        uniform float u_Extent;
        uniform int u_XYPlane;

        out vec4 o_Color;

        // How much of this pixel a grid line with the given spacing covers (0..1).
        float GridLine(vec2 coord, float spacing)
        {
            vec2 scaled = coord / spacing;
            vec2 width = max(fwidth(scaled), vec2(1e-6));
            vec2 distanceToLine = abs(fract(scaled - 0.5) - 0.5) / width;
            float line = 1.0 - min(min(distanceToLine.x, distanceToLine.y), 1.0);

            // Lines closer together than a pixel would shimmer, so fade them out.
            float fade = 1.0 - smoothstep(0.3, 0.8, max(width.x, width.y));
            return line * fade;
        }

        void main()
        {
            vec2 coord  = (u_XYPlane == 0) ? v_WorldPos.xz : v_WorldPos.xy;
            vec2 centre = (u_XYPlane == 0) ? u_Center.xz   : u_Center.xy;

            float minorLine = GridLine(coord, 1.0);
            float majorLine = GridLine(coord, 10.0);

            vec3 color = vec3(0.65);
            float alpha = max(minorLine * 0.2, majorLine * 0.4);

            // The line along X (coord.y == 0) is red. The other axis is blue on the
            // ground and green in the 2D view.
            vec2 pixelWidth = max(fwidth(coord), vec2(1e-6));
            float xAxis = 1.0 - min(abs(coord.y) / pixelWidth.y, 1.0);
            float yAxis = 1.0 - min(abs(coord.x) / pixelWidth.x, 1.0);
            vec3 xAxisColor = vec3(0.9, 0.25, 0.25);
            vec3 yAxisColor = (u_XYPlane == 0) ? vec3(0.3, 0.45, 0.95) : vec3(0.35, 0.85, 0.35);

            color = mix(color, xAxisColor, xAxis);
            color = mix(color, yAxisColor, yAxis);
            alpha = max(alpha, max(xAxis, yAxis) * 0.8);

            // Fade towards the edge of the quad so its border is never visible.
            alpha *= 1.0 - smoothstep(0.5 * u_Extent, u_Extent, length(coord - centre));

            if (alpha < 0.002)
                discard;

            o_Color = vec4(color, alpha);
        }
    )";

    OpenGLShader& GetGridShader()
    {
        static OpenGLShader* shader = new OpenGLShader("Grid", k_GridVertexSource, k_GridFragmentSource);
        return *shader;
    }

    // A unit quad in the XZ plane; the vertex shader scales and places it. Never freed, for the
    // same reason as the other GL statics.
    std::shared_ptr<OpenGLVertexArray>& GetGridQuad()
    {
        static auto* quad = []
        {
            float vertices[] = {
                -1.0f, 0.0f, -1.0f,
                1.0f, 0.0f, -1.0f,
                1.0f, 0.0f,  1.0f,
                -1.0f, 0.0f,  1.0f,
            };
            uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

            auto vertexArray = std::make_shared<OpenGLVertexArray>();

            auto vertexBuffer = std::make_shared<OpenGLVertexBuffer>(vertices, sizeof(vertices));
            vertexBuffer->SetLayout({ { ShaderDataType::Float3, "a_Position" } });
            vertexArray->AddVertexBuffer(vertexBuffer);

            vertexArray->SetIndexBuffer(std::make_shared<OpenGLIndexBuffer>(indices, 6));
            return new std::shared_ptr<OpenGLVertexArray>(vertexArray);
        }();
        return *quad;
    }

    const char* k_SpriteVertexSource = R"(
        #version 450 core
        layout(location = 0) in vec3 a_Position;
        layout(location = 1) in vec2 a_TexCoord;

        uniform mat4 u_ViewProjection;
        uniform mat4 u_Model;
        uniform vec2 u_UVScale;
        uniform vec2 u_UVOffset;

        out vec2 v_TexCoord;

        void main()
        {
            v_TexCoord = a_TexCoord * u_UVScale + u_UVOffset;
            gl_Position = u_ViewProjection * u_Model * vec4(a_Position, 1.0);
        }
    )";

    const char* k_SpriteFragmentSource = R"(
        #version 450 core
        in vec2 v_TexCoord;

        uniform sampler2D u_Texture;
        uniform int u_HasTexture;
        uniform vec4 u_Color;

        out vec4 o_Color;

        void main()
        {
            vec4 color = u_Color;
            if (u_HasTexture != 0)
                color *= texture(u_Texture, v_TexCoord);

            if (color.a < 0.001)
                discard;

            // Unlit. Gamma, because the framebuffer isn't sRGB (same as the PBR shader).
            o_Color = vec4(pow(color.rgb, vec3(1.0 / 2.2)), color.a);
        }
    )";

    OpenGLShader& GetSpriteShader()
    {
        static OpenGLShader* shader = new OpenGLShader("Sprite", k_SpriteVertexSource, k_SpriteFragmentSource);
        return *shader;
    }

    // A unit quad in the XY plane, centred on the origin. Never freed, like the other GL statics.
    std::shared_ptr<OpenGLVertexArray>& GetSpriteQuad()
    {
        static auto* quad = []
        {
            float vertices[] = {
                // position            uv
                -0.5f, -0.5f, 0.0f,    0.0f, 0.0f,
                    0.5f, -0.5f, 0.0f,    1.0f, 0.0f,
                    0.5f,  0.5f, 0.0f,    1.0f, 1.0f,
                -0.5f,  0.5f, 0.0f,    0.0f, 1.0f,
            };
            uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };

            auto vertexArray = std::make_shared<OpenGLVertexArray>();

            auto vertexBuffer = std::make_shared<OpenGLVertexBuffer>(vertices, sizeof(vertices));
            vertexBuffer->SetLayout({
                { ShaderDataType::Float3, "a_Position" },
                { ShaderDataType::Float2, "a_TexCoord" }
            });
            vertexArray->AddVertexBuffer(vertexBuffer);

            vertexArray->SetIndexBuffer(std::make_shared<OpenGLIndexBuffer>(indices, 6));
            return new std::shared_ptr<OpenGLVertexArray>(vertexArray);
        }();
        return *quad;
    }

    // Sampler objects override a texture's own filtering for one draw, so the same texture
    // can be smooth for one sprite and pixel-sharp for another.
    GLuint GetSpriteSampler(bool pixelArt)
    {
        static GLuint samplers[2] = { 0, 0 };
        if (samplers[0] == 0)
        {
            glCreateSamplers(2, samplers);
            for (int i = 0; i < 2; ++i)
            {
                glSamplerParameteri(samplers[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glSamplerParameteri(samplers[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            }
            glSamplerParameteri(samplers[0], GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
            glSamplerParameteri(samplers[0], GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glSamplerParameteri(samplers[1], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glSamplerParameteri(samplers[1], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        }
        return samplers[pixelArt ? 1 : 0];
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

            const MaterialComponent* materials = registry.try_get<MaterialComponent>(entity);

            // The transform is the same for the whole entity, so it is set once.
            const glm::mat4 model = transform.GetMatrix();
            shader.SetMat4("u_Model", model);
            shader.SetMat3("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(model))));

            // One draw call per submesh, each with its own material.
            const auto& submeshes = meshData->GetSubmeshes();
            for (size_t i = 0; i < submeshes.size(); ++i)
            {
                const MaterialSlot* slot = materials ? materials->FindSlot(static_cast<int>(i)) : nullptr;
                ApplyMaterial(shader, slot ? *slot : DefaultMaterial());

                OpenGLRender::DrawIndexed(meshData->GetVertexArray(), submeshes[i].IndexCount, submeshes[i].IndexOffset);
            }
        }
    }

    void WorldRenderer::RenderGrid(const glm::mat4& view, const glm::mat4& projection,
                                   const glm::vec3& cameraPosition, bool xyPlane)
    {
        OpenGLShader& shader = GetGridShader();
        shader.Bind();

        // Centred under the camera so the grid never runs out; the pattern is computed from
        // world position, so it doesn't slide as the quad follows.
        const glm::vec3 center = xyPlane
            ? glm::vec3(cameraPosition.x, cameraPosition.y, 0.0f)
            : glm::vec3(cameraPosition.x, 0.0f, cameraPosition.z);

        shader.SetMat4("u_ViewProjection", projection * view);
        shader.SetFloat3("u_Center", center);
        shader.SetFloat("u_Extent", 200.0f);
        shader.SetInt("u_XYPlane", xyPlane ? 1 : 0);

        // Blend over the scene, test against its depth so objects hide the grid, don't write depth.
        // The separate alpha function keeps the framebuffer's alpha at 1; with the plain function
        // the lines would lower it, and ImGui would then draw the viewport see-through there.
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glDepthMask(GL_FALSE);

        OpenGLRender::DrawIndexed(GetGridQuad());

        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    glm::vec2 WorldRenderer::GetSpriteSize(const SpriteComponent& sprite)
    {
        if (sprite.TexturePath.empty())
            return glm::vec2(1.0f);

        std::shared_ptr<OpenGLTexture2D> texture = TextureLibrary::Get(sprite.TexturePath, true);
        if (!texture)
            return glm::vec2(1.0f);

        const float pixelsPerUnit = sprite.PixelsPerUnit > 0.0f ? sprite.PixelsPerUnit : 100.0f;
        return glm::vec2(texture->GetWidth(), texture->GetHeight()) / pixelsPerUnit;
    }

    void WorldRenderer::RenderSprites(World& world, const glm::mat4& view, const glm::mat4& projection)
    {
        struct SpriteDraw
        {
            const TransformComponent* Transform;
            const SpriteComponent* Sprite;
            float Depth;   // view-space z: more negative = farther from the camera
        };

        std::vector<SpriteDraw> draws;
        auto sprites = world.GetRegistry().view<TransformComponent, SpriteComponent>();
        for (auto [entity, transform, sprite] : sprites.each())
        {
            const glm::vec4 viewPosition = view * glm::vec4(transform.Position, 1.0f);
            draws.push_back({ &transform, &sprite, viewPosition.z });
        }

        if (draws.empty())
            return;

        // Sorting order first, then back to front. Stable, so equal sprites keep a steady order.
        std::stable_sort(draws.begin(), draws.end(), [](const SpriteDraw& a, const SpriteDraw& b)
        {
            if (a.Sprite->SortingOrder != b.Sprite->SortingOrder)
                return a.Sprite->SortingOrder < b.Sprite->SortingOrder;
            return a.Depth < b.Depth;
        });

        OpenGLShader& shader = GetSpriteShader();
        shader.Bind();
        shader.SetMat4("u_ViewProjection", projection * view);
        shader.SetInt("u_Texture", 0);

        /* Blend over what's there and test against the 3D depth, but don't write depth, so sprites
           never hide each other (that's the sorting's job). The separate alpha function keeps the
           framebuffer's alpha at 1 so ImGui doesn't draw the viewport see-through. */
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
        glDepthMask(GL_FALSE);

        for (const SpriteDraw& draw : draws)
        {
            const SpriteComponent& sprite = *draw.Sprite;

            std::shared_ptr<OpenGLTexture2D> texture;
            if (!sprite.TexturePath.empty())
                texture = TextureLibrary::Get(sprite.TexturePath, true);

            const glm::vec2 size = GetSpriteSize(sprite);
            const glm::mat4 model = draw.Transform->GetMatrix() * glm::scale(glm::mat4(1.0f), glm::vec3(size, 1.0f));

            shader.SetMat4("u_Model", model);
            shader.SetFloat4("u_Color", sprite.Color);
            shader.SetInt("u_HasTexture", texture ? 1 : 0);

            // Flipping is a UV mirror: u' = 1 - u is scale -1, offset 1.
            shader.SetFloat2("u_UVScale", glm::vec2(sprite.FlipX ? -1.0f : 1.0f, sprite.FlipY ? -1.0f : 1.0f));
            shader.SetFloat2("u_UVOffset", glm::vec2(sprite.FlipX ? 1.0f : 0.0f, sprite.FlipY ? 1.0f : 0.0f));

            if (texture)
            {
                texture->Bind(0);
                glBindSampler(0, GetSpriteSampler(sprite.PixelArt));
            }

            OpenGLRender::DrawIndexed(GetSpriteQuad());
        }

        glBindSampler(0, 0);   // so the next mesh pass sees the textures' own filtering again
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
}
