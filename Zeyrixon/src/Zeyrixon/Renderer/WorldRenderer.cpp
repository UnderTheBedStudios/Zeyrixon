#include <pch.h>

#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/Renderer/TextureLibrary.h>
#include <Zeyrixon/World/Components.h>

#include <Platform/OpenGL/OpenGLRender.h>
#include <Platform/OpenGL/OpenGLShader.h>
#include <Platform/OpenGL/OpenGLUniformBuffer.h>
#include <Zeyrixon/Core/Log.h>

#include <glad/glad.h>

#include <algorithm>
#include <cmath>
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
            layout(location = 3) in vec4 a_Joints;    // skinned meshes only: joint indices, stored as floats
            layout(location = 4) in vec4 a_Weights;   // skinned meshes only: how much each joint pulls

            uniform mat4 u_ViewProjection;
            uniform mat4 u_Model;
            uniform mat3 u_NormalMatrix;

            uniform int u_Skinned;                    // 1 = deform with u_JointMatrices first
            uniform mat4 u_JointMatrices[128];        // keep in step with ModelData::MaxJoints

            out vec3 v_WorldPos;
            out vec3 v_Normal;
            out vec2 v_TexCoord;

            void main()
            {
                vec4 localPosition = vec4(a_Position, 1.0);
                vec3 localNormal = a_Normal;

                if (u_Skinned != 0)
                {
                    // Blend the matrices of the (up to) four joints that hold this vertex.
                    mat4 skin = a_Weights.x * u_JointMatrices[int(a_Joints.x + 0.5)]
                              + a_Weights.y * u_JointMatrices[int(a_Joints.y + 0.5)]
                              + a_Weights.z * u_JointMatrices[int(a_Joints.z + 0.5)]
                              + a_Weights.w * u_JointMatrices[int(a_Joints.w + 0.5)];
                    localPosition = skin * localPosition;
                    localNormal = mat3(skin) * localNormal;
                }

                vec4 world = u_Model * localPosition;
                v_WorldPos = world.xyz;
                v_Normal = u_NormalMatrix * localNormal;
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
            uniform float u_AmbientIntensity;   // 0 = no ambient light at all

            // Shadows: one depth map, rendered from the shadow-casting directional light.
            uniform sampler2DShadow u_ShadowMap;
            uniform mat4 u_ShadowMatrix;        // world space -> shadow-map UV + depth (all 0..1)
            uniform vec4 u_ShadowParams;        // x = world size of one texel, y = depth range, z = depth bias (world), w = normal offset (texels)
            uniform int u_ShadowLightIndex;     // which entry of u_Lights casts the shadow, -1 = none

            // Keep MAX_LIGHTS and the layout in step with WorldRenderer's GPULight / LightBlock.
            // Every member is a vec4 so std140 padding never gets in the way.
            #define MAX_LIGHTS 64
            struct Light
            {
                vec4 PositionRange;    // xyz = world position, w = range
                vec4 DirectionType;    // xyz = direction the light travels, w = 0 directional / 1 point / 2 spot
                vec4 ColorIntensity;   // rgb = colour * intensity (already multiplied on the CPU)
                vec4 SpotCones;        // x = cos(inner angle), y = cos(outer angle)
            };

            layout(std140, binding = 0) uniform LightBlock
            {
                ivec4 u_LightCounts;   // x = how many entries of u_Lights are in use
                Light u_Lights[MAX_LIGHTS];
            };

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
            uniform int u_PackedMR;          // 1 = roughness (green) and metallic (blue) are both in u_RoughnessMap

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

            // 1 = fully lit, 0 = fully in shadow, for the shadow-casting directional light.
            float ShadowFactor(vec3 N, vec3 L)
            {
                float NdotL = clamp(dot(N, L), 0.0, 1.0);

                // Normal offset: slide the lookup point off the surface, more the more edge-on the
                // surface is to the light. This is what stops a surface shadowing itself.
                vec3 position = v_WorldPos + N * (u_ShadowParams.x * u_ShadowParams.w * (1.0 - NdotL));

                vec4 shadowCoord = u_ShadowMatrix * vec4(position, 1.0);
                vec3 p = shadowCoord.xyz / shadowCoord.w;

                // Outside the shadow map there is no information, so treat it as lit.
                if (p.x < 0.0 || p.x > 1.0 || p.y < 0.0 || p.y > 1.0 || p.z > 1.0)
                    return 1.0;

                float reference = p.z - u_ShadowParams.z / u_ShadowParams.y;   // the bias, converted to depth units

                // 3x3 percentage-closer filtering. Each texture() call on a sampler2DShadow compares
                // the reference to the stored depth in hardware (and bilinearly blends 2x2 of them).
                vec2 texel = 1.0 / vec2(textureSize(u_ShadowMap, 0));
                float lit = 0.0;
                for (int x = -1; x <= 1; ++x)
                    for (int y = -1; y <= 1; ++y)
                        lit += texture(u_ShadowMap, vec3(p.xy + vec2(x, y) * texel, reference));
                return lit / 9.0;
            }

            // The light that arrives at this pixel from ONE light, run through Cook-Torrance
            // specular + Lambert diffuse. Called once per light and the results are added up.
            vec3 EvaluateLight(Light light, float shadow, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness, vec3 F0)
            {
                int type = int(light.DirectionType.w + 0.5);

                vec3 L;                  // from the pixel towards the light
                float attenuation = 1.0; // how much of the light's strength survives to get here

                if (type == 0)
                {
                    // Directional: the same direction everywhere, and it never fades.
                    L = normalize(-light.DirectionType.xyz);
                }
                else
                {
                    vec3 toLight = light.PositionRange.xyz - v_WorldPos;
                    float dist2 = max(dot(toLight, toLight), 1e-4);
                    float dist = sqrt(dist2);
                    L = toLight / dist;

                    // Inverse-square falloff, times a window that eases it to exactly zero at
                    // the light's range (so the light has a finite reach we can cull by).
                    float range = light.PositionRange.w;
                    float window = clamp(1.0 - pow(dist / range, 4.0), 0.0, 1.0);
                    attenuation = (window * window) / (dist2 + 1.0);

                    if (type == 2)
                    {
                        // Spot: 1 inside the inner cone, 0 outside the outer one, smooth in between.
                        float cosAngle = dot(-L, normalize(light.DirectionType.xyz));
                        float t = clamp((cosAngle - light.SpotCones.y) / max(light.SpotCones.x - light.SpotCones.y, 1e-4), 0.0, 1.0);
                        attenuation *= t * t;
                    }
                }

                float NdotL = max(dot(N, L), 0.0);
                if (attenuation <= 0.0 || NdotL <= 0.0)
                    return vec3(0.0);

                vec3 H = normalize(V + L);
                float NDF = DistributionGGX(N, H, roughness);
                float G = GeometrySmith(N, V, L, roughness);
                vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

                vec3 specular = (NDF * G * F)
                              / (4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001);
                vec3 kD = (vec3(1.0) - F) * (1.0 - metallic);

                vec3 radiance = light.ColorIntensity.rgb * attenuation * shadow;
                return (kD * albedo / PI + specular) * radiance * NdotL;
            }

            void main()
            {
                vec3 albedo = u_Albedo;
                if (u_HasAlbedoMap != 0)
                    albedo *= texture(u_AlbedoMap, v_TexCoord).rgb;

                float metallic, roughness;
                if (u_PackedMR != 0 && u_HasRoughnessMap != 0)
                {
                    // glTF: one texture, roughness in G, metallic in B; the factors scale it.
                    vec3 packedMR = texture(u_RoughnessMap, v_TexCoord).rgb;
                    roughness = u_Roughness * packedMR.g;
                    metallic  = u_Metallic  * packedMR.b;
                }
                else
                {
                    metallic  = (u_HasMetallicMap  != 0) ? texture(u_MetallicMap,  v_TexCoord).r : u_Metallic;
                    roughness = (u_HasRoughnessMap != 0) ? texture(u_RoughnessMap, v_TexCoord).r : u_Roughness;
                }
                float ao = (u_HasAOMap != 0) ? texture(u_AOMap, v_TexCoord).r : u_AO;
                roughness = clamp(roughness, 0.04, 1.0);   // a perfectly smooth surface gives a degenerate highlight

                vec3 N = GetNormal();
                vec3 V = normalize(u_CameraPosition - v_WorldPos);

                vec3 F0 = mix(vec3(0.04), albedo, metallic);

                // Every light adds its own contribution; light is additive, so a plain sum is right.
                vec3 Lo = vec3(0.0);
                int lightCount = min(u_LightCounts.x, MAX_LIGHTS);
                for (int i = 0; i < lightCount; ++i)
                {
                    float shadow = 1.0;
                    if (i == u_ShadowLightIndex)
                        shadow = ShadowFactor(N, normalize(-u_Lights[i].DirectionType.xyz));
                    Lo += EvaluateLight(u_Lights[i], shadow, N, V, albedo, metallic, roughness, F0);
                }

                // Hemisphere ambient: a stand-in until image-based lighting exists.
                vec3 hemisphere = mix(vec3(0.15, 0.13, 0.12), vec3(0.45, 0.55, 0.70), N.y * 0.5 + 0.5);
                vec3 ambient = ((1.0 - metallic) * albedo * hemisphere
                              + F0 * hemisphere * (1.0 - roughness)) * ao * u_AmbientIntensity;

                vec3 color = ambient + Lo;

                color = color / (color + vec3(1.0));   // Reinhard tone mapping
                color = pow(color, vec3(1.0 / 2.2));   // the framebuffer is not sRGB
                o_Color = vec4(color, 1.0);
            }
        )";

        // ---- Lights -------------------------------------------------------------------------

        constexpr int k_MaxLights = 64;   // keep in step with MAX_LIGHTS in the fragment shader

        // These two structs must match the shader's std140 block byte for byte. Everything is a
        // vec4 (16 bytes), so there is no hidden padding to get wrong.
        struct GPULight
        {
            glm::vec4 PositionRange;    // xyz position, w range
            glm::vec4 DirectionType;    // xyz travel direction, w type
            glm::vec4 ColorIntensity;   // rgb = colour * intensity
            glm::vec4 SpotCones;        // x cos(inner), y cos(outer)
        };

        struct LightBlock
        {
            glm::ivec4 Counts;          // x = light count
            GPULight Lights[k_MaxLights];
        };

        static_assert(sizeof(GPULight) == 64, "GPULight must be 64 bytes to match std140");
        static_assert(sizeof(LightBlock) == 16 + 64 * k_MaxLights, "LightBlock must match the shader's std140 block");

        // Created on first use (needs the GL context) and never freed, like the shaders.
        OpenGLUniformBuffer& GetLightBuffer()
        {
            static OpenGLUniformBuffer* buffer = new OpenGLUniformBuffer(sizeof(LightBlock), 0);   // binding 0
            return *buffer;
        }

        // What UploadLights found out about the light that should cast the shadow (if any).
        struct ShadowCaster
        {
            bool Valid = false;
            int LightIndex = -1;          // position in the uploaded light array
            glm::vec3 Direction{ 0.f, -1.f, 0.f };
            float Distance = 50.0f;
            float Bias = 0.05f;
            float NormalBias = 1.5f;
        };

        GPULight MakeGPULight(const LightComponent& light, const TransformComponent& transform)
        {
            GPULight gpu{};
            const float range = std::max(light.Range, 0.01f);

            // Cone angles are edited in degrees. The shader compares cosines, which is cheaper than
            // taking an acos per pixel. Keep inner <= outer and below 90 degrees.
            const float outer = glm::clamp(light.OuterConeAngle, 1.0f, 89.0f);
            const float inner = glm::clamp(light.InnerConeAngle, 0.0f, outer);

            gpu.PositionRange  = glm::vec4(transform.Position, range);
            gpu.DirectionType  = glm::vec4(LightComponent::GetDirection(transform), static_cast<float>(light.Type));
            gpu.ColorIntensity = glm::vec4(light.Color * std::max(light.Intensity, 0.0f), 0.0f);
            gpu.SpotCones      = glm::vec4(std::cos(glm::radians(inner)), std::cos(glm::radians(outer)), 0.0f, 0.0f);
            return gpu;
        }

        /* Collects every Light component of the world into the uniform buffer (binding 0).
           Forward rendering: all lights go to the GPU once per frame and every pixel loops over them.
           If the world has more than k_MaxLights, directional lights win, then the nearest ones. */
        ShadowCaster UploadLights(World& world, const glm::vec3& cameraPosition)
        {
            struct Candidate
            {
                GPULight Light;
                bool Directional;
                float DistanceSquared;   // to the camera; used only when there are too many lights
                const LightComponent* Source;   // null for the built-in sun
                glm::vec3 Direction;
            };

            std::vector<Candidate> candidates;
            auto lights = world.GetRegistry().view<TransformComponent, LightComponent>();
            for (auto [entity, transform, light] : lights.each())
            {
                const glm::vec3 offset = transform.Position - cameraPosition;
                candidates.push_back({ MakeGPULight(light, transform),
                                       light.Type == LightType::Directional,
                                       glm::dot(offset, offset),
                                       &light,
                                       LightComponent::GetDirection(transform) });
            }

            // A world with no lights would be black, so give it a default sun. It disappears as soon
            // as the first Light component is added.
            if (candidates.empty())
            {
                GPULight sun{};
                sun.PositionRange  = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
                sun.DirectionType  = glm::vec4(-glm::normalize(glm::vec3(0.4f, 0.8f, 0.6f)), 0.0f);
                sun.ColorIntensity = glm::vec4(3.0f, 2.9f, 2.7f, 0.0f);
                candidates.push_back({ sun, true, 0.0f, nullptr, glm::vec3(sun.DirectionType) });
            }

            if (candidates.size() > static_cast<size_t>(k_MaxLights))
            {
                static bool warned = false;
                if (!warned)
                {
                    Z_CORE_WARN("World has {0} lights; only {1} are used (directional first, then nearest to the camera)",
                                candidates.size(), k_MaxLights);
                    warned = true;
                }

                std::stable_sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b)
                {
                    if (a.Directional != b.Directional)
                        return a.Directional;
                    return a.DistanceSquared < b.DistanceSquared;
                });
                candidates.resize(k_MaxLights);
            }

            LightBlock block{};
            block.Counts = glm::ivec4(static_cast<int>(candidates.size()), 0, 0, 0);
            for (size_t i = 0; i < candidates.size(); ++i)
                block.Lights[i] = candidates[i].Light;

            // Only the used part is sent: the header plus `count` lights.
            GetLightBuffer().SetData(&block, static_cast<uint32_t>(sizeof(glm::ivec4) + candidates.size() * sizeof(GPULight)));

            // The first directional light with CastShadows on is the one that casts the shadow.
            ShadowCaster caster;
            for (size_t i = 0; i < candidates.size(); ++i)
            {
                const Candidate& c = candidates[i];
                if (c.Source && c.Directional && c.Source->CastShadows)
                {
                    caster.Valid = true;
                    caster.LightIndex = static_cast<int>(i);
                    caster.Direction = c.Direction;
                    caster.Distance = std::max(c.Source->ShadowDistance, 1.0f);
                    caster.Bias = std::max(c.Source->ShadowBias, 0.0f);
                    caster.NormalBias = std::max(c.Source->ShadowNormalBias, 0.0f);
                    break;
                }
            }
            return caster;
        }

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
            shader.SetInt("u_PackedMR", material.PackedMetallicRoughness ? 1 : 0);

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

    namespace
    {
        // ---- Shadow map ---------------------------------------------------------------------

        constexpr int k_ShadowMapSize = 2048;   // texels per side. 4096 is sharper but 4x the memory and fill cost
        constexpr int k_ShadowTextureSlot = 5;  // texture units 0-4 are the material maps

        // Depth only: the same position and skinning as the PBR vertex shader, no colour output at all.
        const char* k_ShadowVertexSource = R"(
            #version 450 core
            layout(location = 0) in vec3 a_Position;
            layout(location = 3) in vec4 a_Joints;
            layout(location = 4) in vec4 a_Weights;

            uniform mat4 u_LightViewProjection;
            uniform mat4 u_Model;
            uniform int u_Skinned;
            uniform mat4 u_JointMatrices[128];   // keep in step with ModelData::MaxJoints

            void main()
            {
                vec4 localPosition = vec4(a_Position, 1.0);
                if (u_Skinned != 0)
                {
                    mat4 skin = a_Weights.x * u_JointMatrices[int(a_Joints.x + 0.5)]
                              + a_Weights.y * u_JointMatrices[int(a_Joints.y + 0.5)]
                              + a_Weights.z * u_JointMatrices[int(a_Joints.z + 0.5)]
                              + a_Weights.w * u_JointMatrices[int(a_Joints.w + 0.5)];
                    localPosition = skin * localPosition;
                }
                gl_Position = u_LightViewProjection * u_Model * localPosition;
            }
        )";

        const char* k_ShadowFragmentSource = R"(
            #version 450 core
            void main() {}   // depth is written automatically
        )";

        OpenGLShader& GetShadowShader()
        {
            static OpenGLShader* shader = new OpenGLShader("Shadow", k_ShadowVertexSource, k_ShadowFragmentSource);
            return *shader;
        }

        struct ShadowResources
        {
            GLuint Framebuffer = 0;
            GLuint DepthTexture = 0;
        };

        // Created on first use (needs the GL context) and never freed, like the other GL statics.
        ShadowResources& GetShadowResources()
        {
            static ShadowResources* resources = []
            {
                auto* r = new ShadowResources();

                glCreateTextures(GL_TEXTURE_2D, 1, &r->DepthTexture);
                glTextureStorage2D(r->DepthTexture, 1, GL_DEPTH_COMPONENT32F, k_ShadowMapSize, k_ShadowMapSize);
                glTextureParameteri(r->DepthTexture, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                glTextureParameteri(r->DepthTexture, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                glTextureParameteri(r->DepthTexture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
                glTextureParameteri(r->DepthTexture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
                const float border[4] = { 1.0f, 1.0f, 1.0f, 1.0f };   // outside the map = "nothing in the way" = lit
                glTextureParameterfv(r->DepthTexture, GL_TEXTURE_BORDER_COLOR, border);

                // Makes the sampler2DShadow in the PBR shader compare for us instead of returning raw depth.
                glTextureParameteri(r->DepthTexture, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
                glTextureParameteri(r->DepthTexture, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

                glCreateFramebuffers(1, &r->Framebuffer);
                glNamedFramebufferTexture(r->Framebuffer, GL_DEPTH_ATTACHMENT, r->DepthTexture, 0);
                glNamedFramebufferDrawBuffer(r->Framebuffer, GL_NONE);   // no colour attachment
                glNamedFramebufferReadBuffer(r->Framebuffer, GL_NONE);

                if (glCheckNamedFramebufferStatus(r->Framebuffer, GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
                    Z_CORE_ERROR("Shadow map framebuffer is incomplete");

                return r;
            }();
            return *resources;
        }

        /* Draws every mesh with the given shader. depthOnly = true is the shadow pass: no materials,
           no normal matrix, just positions (and skinning). Shared so both passes pose animated models
           exactly the same way. */
        void DrawMeshes(World& world, OpenGLShader& shader, bool depthOnly)
        {
        auto& registry = world.GetRegistry();
        auto entities = registry.view<TransformComponent, MeshComponent>();
        for (auto [entity, transform, mesh] : entities.each())
        {
            std::shared_ptr<Mesh> meshData = MeshLibrary::Get(mesh.MeshPath);
            if (!meshData)
                continue;

            const MaterialComponent* materials = registry.try_get<MaterialComponent>(entity);
            const std::vector<MaterialSlot>& importedMaterials = meshData->GetImportedMaterials();

            /* glTF models: work out the pose to draw. With an Animation component that is the chosen clip at
               its current time; without one it's the rest pose, which was computed when the model loaded. */
            const ModelData* modelData = meshData->GetModel().get();
            ModelPose animatedPose;
            const ModelPose* pose = nullptr;
            if (modelData)
            {
                const AnimationComponent* animation = registry.try_get<AnimationComponent>(entity);
                if (animation && animation->ClipIndex >= 0 && animation->ClipIndex < static_cast<int>(modelData->Animations.size()))
                {
                    modelData->Evaluate(animation->ClipIndex, animation->Time, animatedPose);
                    pose = &animatedPose;
                }
                else
                {
                    pose = &modelData->RestPose;
                }
            }

            const glm::mat4 model = transform.GetMatrix();
            int uploadedSkin = -1;   // which skin's joint matrices are currently in the shader (for this entity)

            // One draw call per submesh, each with its own material.
            const auto& submeshes = meshData->GetSubmeshes();
            for (size_t i = 0; i < submeshes.size(); ++i)
            {
                const Submesh& submesh = submeshes[i];

                // The Material component wins; otherwise the file's own material; otherwise plain grey.
                // The shadow pass only needs shapes, so it skips materials.
                if (!depthOnly)
                {
                    const MaterialSlot* slot = materials ? materials->FindSlot(static_cast<int>(i)) : nullptr;
                    if (!slot && !materials && i < importedMaterials.size())
                        slot = &importedMaterials[i];
                    ApplyMaterial(shader, slot ? *slot : DefaultMaterial());
                }

                glm::mat4 submeshModel = model;
                int skin = -1;
                if (pose)
                {
                    if (submesh.SkinIndex >= 0 && submesh.SkinIndex < static_cast<int>(pose->JointMatrices.size()))
                    {
                        // Skinned: the joint matrices place the vertices, so the entity's own transform is all that's left.
                        skin = submesh.SkinIndex;
                    }
                    else if (submesh.NodeIndex >= 0 && submesh.NodeIndex < static_cast<int>(pose->NodeMatrices.size()))
                    {
                        /* An animated rigid part: its vertices were baked at the node's REST position, so move
                           them by how far the node has travelled from there. */
                        submeshModel = model * pose->NodeMatrices[submesh.NodeIndex] * modelData->InverseRestMatrices[submesh.NodeIndex];
                    }
                }

                shader.SetInt("u_Skinned", skin >= 0 ? 1 : 0);
                if (skin >= 0 && skin != uploadedSkin)
                {
                    const auto& matrices = pose->JointMatrices[skin];
                    const uint32_t count = static_cast<uint32_t>(std::min<size_t>(matrices.size(), ModelData::MaxJoints));
                    if (count > 0)
                        shader.SetMat4Array("u_JointMatrices", matrices.data(), count);
                    uploadedSkin = skin;
                }

                shader.SetMat4("u_Model", submeshModel);
                if (!depthOnly)
                    shader.SetMat3("u_NormalMatrix", glm::transpose(glm::inverse(glm::mat3(submeshModel))));

                OpenGLRender::DrawIndexed(meshData->GetVertexArray(), submesh.IndexCount, submesh.IndexOffset);
            }
        }
        }

        /* Renders the scene's depth as seen by the shadow-casting light into the shadow map.
           Returns the matrix that takes a world position to shadow-map UV + depth (all 0..1).
           Restores the framebuffer, viewport and GL state it changed, so the caller carries on as before. */
        glm::mat4 RenderShadowMap(World& world, const ShadowCaster& caster, const glm::vec3& cameraPosition)
        {
            ShadowResources& resources = GetShadowResources();

            // ---- Where the light's "camera" goes ----
            // An orthographic box (a sun's rays are parallel) around the camera, R units to each side.
            const float R = caster.Distance;
            const glm::vec3 direction = glm::normalize(caster.Direction);
            const glm::vec3 up = std::abs(direction.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);

            // View = rotate the world so the light looks down -Z, then slide so the box is centred.
            const glm::mat4 lightRotation = glm::lookAt(glm::vec3(0.f), direction, up);

            /* Snap the box's centre to whole texels. Without this the shadow map shifts by fractions of a
               texel as the camera moves, and every shadow edge shimmers. */
            glm::vec3 center = glm::vec3(lightRotation * glm::vec4(cameraPosition, 1.0f));
            const float texelSize = (2.0f * R) / static_cast<float>(k_ShadowMapSize);
            center.x = std::floor(center.x / texelSize) * texelSize;
            center.y = std::floor(center.y / texelSize) * texelSize;

            // The "eye" sits R behind the centre; the box reaches R behind the eye's far side too.
            const glm::mat4 lightView = glm::translate(glm::mat4(1.0f), -glm::vec3(center.x, center.y, center.z + R)) * lightRotation;
            const glm::mat4 lightProjection = glm::ortho(-R, R, -R, R, 0.0f, 2.0f * R);
            const glm::mat4 lightViewProjection = lightProjection * lightView;

            // ---- Remember what to restore ----
            GLint previousFramebuffer = 0;
            GLint previousViewport[4] = {};
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousFramebuffer);
            glGetIntegerv(GL_VIEWPORT, previousViewport);
            const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);

            glBindFramebuffer(GL_FRAMEBUFFER, resources.Framebuffer);
            glViewport(0, 0, k_ShadowMapSize, k_ShadowMapSize);
            glDepthMask(GL_TRUE);
            glClear(GL_DEPTH_BUFFER_BIT);

            glDisable(GL_CULL_FACE);              // thin walls and foliage have no "back" worth skipping
            glEnable(GL_DEPTH_CLAMP);             // things between the light and the box still cast, instead of being clipped away
            glEnable(GL_POLYGON_OFFSET_FILL);     // pushes stored depth slightly back so surfaces don't shadow themselves
            glPolygonOffset(2.0f, 4.0f);

            OpenGLShader& shader = GetShadowShader();
            shader.Bind();
            shader.SetMat4("u_LightViewProjection", lightViewProjection);
            DrawMeshes(world, shader, true);

            // ---- Put everything back ----
            glDisable(GL_POLYGON_OFFSET_FILL);
            glDisable(GL_DEPTH_CLAMP);
            if (cullWasEnabled)
                glEnable(GL_CULL_FACE);
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previousFramebuffer));
            glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);

            // NDC is -1..1; texture coordinates and stored depth are 0..1. This maps one to the other.
            const glm::mat4 toTextureSpace = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
            return toTextureSpace * lightViewProjection;
        }
    }

    void WorldRenderer::RenderWorld(World& world, const glm::mat4& view, const glm::mat4& projection,
                                    const glm::vec3& cameraPosition)
    {
        // All lights of the world go to the GPU once; every mesh below is lit by all of them.
        const ShadowCaster caster = UploadLights(world, cameraPosition);

        // The shadow pass comes first: it draws the scene from the light into its own depth texture,
        // then hands the framebuffer back exactly as it found it.
        glm::mat4 shadowMatrix(1.0f);
        if (caster.Valid)
            shadowMatrix = RenderShadowMap(world, caster, cameraPosition);

        OpenGLShader& shader = GetShader();
        shader.Bind();

        shader.SetMat4("u_ViewProjection", projection * view);
        shader.SetFloat3("u_CameraPosition", cameraPosition);
        shader.SetFloat("u_AmbientIntensity", world.GetAmbientIntensity());

        // The shadow map lives on its own texture unit; the materials use 0-4.
        shader.SetInt("u_ShadowMap", k_ShadowTextureSlot);
        shader.SetInt("u_ShadowLightIndex", caster.Valid ? caster.LightIndex : -1);
        if (caster.Valid)
        {
            const float texelWorldSize = (2.0f * caster.Distance) / static_cast<float>(k_ShadowMapSize);
            shader.SetMat4("u_ShadowMatrix", shadowMatrix);
            shader.SetFloat4("u_ShadowParams", glm::vec4(texelWorldSize, 2.0f * caster.Distance, caster.Bias, caster.NormalBias));
            glBindTextureUnit(k_ShadowTextureSlot, GetShadowResources().DepthTexture);
        }

        shader.SetInt("u_AlbedoMap", 0);
        shader.SetInt("u_NormalMap", 1);
        shader.SetInt("u_RoughnessMap", 2);
        shader.SetInt("u_MetallicMap", 3);
        shader.SetInt("u_AOMap", 4);

        DrawMeshes(world, shader, false);
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
