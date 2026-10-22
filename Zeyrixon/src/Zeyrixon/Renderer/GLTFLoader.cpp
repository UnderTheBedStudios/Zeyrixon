#include <pch.h>

#include <Zeyrixon/Renderer/GLTFLoader.h>
#include <Zeyrixon/Renderer/TextureLibrary.h>
#include <Zeyrixon/Core/Log.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>

// cgltf is a single-header C library: exactly one .cpp compiles its implementation, and this is it.
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <unordered_map>

namespace Zeyrixon
{
    namespace
    {
        /* Things the conversion functions all need, bundled so they don't take six parameters each. */
        struct ImportContext
        {
            const cgltf_data* Data = nullptr;
            std::string Path;
            std::string Directory;
            GLTFImportResult* Out = nullptr;
            std::unordered_map<size_t, std::string> ImageKeys;   // image index -> texture path/key ("" = unusable)
        };

        // A node given as a 4x4 matrix has to be split into translation / rotation / scale, since
        // animations replace those three individually.
        void DecomposeInto(const glm::mat4& m, ModelNode& node)
        {
            auto nonZero = [](float v) { return std::abs(v) < 1e-8f ? 1.0f : v; };

            node.Translation = glm::vec3(m[3]);

            glm::vec3 scale(glm::length(glm::vec3(m[0])), glm::length(glm::vec3(m[1])), glm::length(glm::vec3(m[2])));
            if (glm::determinant(glm::mat3(m)) < 0.0f)
                scale.x = -scale.x;   // a mirrored node: put the flip on one axis
            scale = glm::vec3(nonZero(scale.x), nonZero(scale.y), nonZero(scale.z));

            const glm::mat3 rotation(glm::vec3(m[0]) / scale.x, glm::vec3(m[1]) / scale.y, glm::vec3(m[2]) / scale.z);
            node.Rotation = glm::normalize(glm::quat_cast(rotation));
            node.Scale = scale;
        }

        void ReadNodes(const cgltf_data* data, ModelData& model)
        {
            model.Nodes.resize(data->nodes_count);
            for (size_t i = 0; i < data->nodes_count; ++i)
            {
                const cgltf_node& source = data->nodes[i];
                ModelNode& node = model.Nodes[i];

                node.Name = source.name ? source.name : "";
                node.Parent = source.parent ? static_cast<int>(cgltf_node_index(data, source.parent)) : -1;
                for (size_t c = 0; c < source.children_count; ++c)
                    node.Children.push_back(static_cast<int>(cgltf_node_index(data, source.children[c])));

                if (source.has_matrix)
                {
                    DecomposeInto(glm::make_mat4(source.matrix), node);
                }
                else
                {
                    if (source.has_translation)
                        node.Translation = glm::vec3(source.translation[0], source.translation[1], source.translation[2]);
                    if (source.has_rotation)   // glTF stores x y z w; glm's constructor wants w x y z
                        node.Rotation = glm::normalize(glm::quat(source.rotation[3], source.rotation[0], source.rotation[1], source.rotation[2]));
                    if (source.has_scale)
                        node.Scale = glm::vec3(source.scale[0], source.scale[1], source.scale[2]);
                }
            }
        }

        void ReadSkins(const cgltf_data* data, ModelData& model)
        {
            for (size_t s = 0; s < data->skins_count; ++s)
            {
                const cgltf_skin& source = data->skins[s];

                ModelSkin skin;
                skin.Name = source.name ? source.name : "";
                for (size_t j = 0; j < source.joints_count; ++j)
                    skin.Joints.push_back(static_cast<int>(cgltf_node_index(data, source.joints[j])));

                // Without an accessor every inverse bind matrix is the identity.
                skin.InverseBindMatrices.assign(skin.Joints.size(), glm::mat4(1.0f));
                if (source.inverse_bind_matrices && source.inverse_bind_matrices->count >= skin.Joints.size())
                {
                    std::vector<float> floats(skin.Joints.size() * 16);
                    cgltf_accessor_unpack_floats(source.inverse_bind_matrices, floats.data(), floats.size());
                    for (size_t j = 0; j < skin.Joints.size(); ++j)
                        skin.InverseBindMatrices[j] = glm::make_mat4(&floats[j * 16]);   // glTF matrices are column-major, like glm
                }

                if (skin.Joints.size() > static_cast<size_t>(ModelData::MaxJoints))
                    Z_CORE_WARN("glTF skin '{0}' has {1} joints; only the first {2} can be used", skin.Name, skin.Joints.size(), ModelData::MaxJoints);

                model.Skins.push_back(std::move(skin));
            }
        }

        void ReadAnimations(const cgltf_data* data, ModelData& model)
        {
            bool warnedAboutMorphTargets = false;

            for (size_t a = 0; a < data->animations_count; ++a)
            {
                const cgltf_animation& source = data->animations[a];

                AnimationClip clip;
                clip.Name = source.name ? source.name : "Animation " + std::to_string(a);

                for (size_t c = 0; c < source.channels_count; ++c)
                {
                    const cgltf_animation_channel& channel = source.channels[c];
                    if (!channel.target_node || !channel.sampler || !channel.sampler->input || !channel.sampler->output)
                        continue;

                    AnimationChannel out;
                    switch (channel.target_path)
                    {
                        case cgltf_animation_path_type_translation: out.Path = AnimationPath::Translation; break;
                        case cgltf_animation_path_type_rotation:    out.Path = AnimationPath::Rotation; break;
                        case cgltf_animation_path_type_scale:       out.Path = AnimationPath::Scale; break;
                        default:
                            // Morph target weights: not supported
                            if (!warnedAboutMorphTargets)
                                Z_CORE_WARN("glTF animation '{0}' animates morph target weights, which aren't supported (ignored)", clip.Name);
                            warnedAboutMorphTargets = true;
                            continue;
                    }

                    switch (channel.sampler->interpolation)
                    {
                        case cgltf_interpolation_type_step:         out.Interpolation = AnimationInterpolation::Step; break;
                        case cgltf_interpolation_type_cubic_spline: out.Interpolation = AnimationInterpolation::CubicSpline; break;
                        default:                                    out.Interpolation = AnimationInterpolation::Linear; break;
                    }

                    out.Node = static_cast<int>(cgltf_node_index(data, channel.target_node));

                    const size_t keyCount = channel.sampler->input->count;
                    if (keyCount == 0)
                        continue;

                    const size_t components = out.Path == AnimationPath::Rotation ? 4 : 3;
                    const size_t floatsPerKey = (out.Interpolation == AnimationInterpolation::CubicSpline ? 3 : 1) * components;

                    // A malformed file could list a different number of values than keys; reading it
                    // anyway would index past the end of the array when sampling.
                    if (cgltf_accessor_unpack_floats(channel.sampler->output, nullptr, 0) != keyCount * floatsPerKey)
                    {
                        Z_CORE_WARN("glTF animation '{0}': a channel's output doesn't match its input (skipped)", clip.Name);
                        continue;
                    }

                    out.Times.resize(keyCount);
                    cgltf_accessor_unpack_floats(channel.sampler->input, out.Times.data(), keyCount);
                    out.Values.resize(keyCount * floatsPerKey);
                    cgltf_accessor_unpack_floats(channel.sampler->output, out.Values.data(), out.Values.size());

                    clip.Duration = std::max(clip.Duration, out.Times.back());
                    clip.Channels.push_back(std::move(out));
                }

                model.Animations.push_back(std::move(clip));
            }
        }

        /* Turns a glTF texture into something TextureLibrary understands: the file's path when the image
           is a separate file, or a "gltf:" key when the image's bytes are inside the .glb (or a base64
           data URI), in which case the bytes are queued in `Out->EmbeddedImages`. Empty = no texture. */
        std::string ResolveTexture(ImportContext& context, const cgltf_texture_view& view)
        {
            const cgltf_texture* texture = view.texture;
            if (!texture || !texture->image)
                return {};

            const cgltf_image* image = texture->image;
            const size_t imageIndex = cgltf_image_index(context.Data, image);

            auto known = context.ImageKeys.find(imageIndex);
            if (known != context.ImageKeys.end())
                return known->second;

            std::string key;
            if (image->uri && std::strncmp(image->uri, "data:", 5) != 0)
            {
                // A separate file next to the model. URIs are percent-encoded ("my%20texture.png").
                std::string uri = image->uri;
                cgltf_decode_uri(&uri[0]);
                uri.resize(std::strlen(uri.c_str()));
                key = (std::filesystem::path(context.Directory) / uri).string();
            }
            else
            {
                std::vector<unsigned char> bytes;

                if (image->buffer_view)
                {
                    const uint8_t* source = cgltf_buffer_view_data(image->buffer_view);
                    if (source)
                        bytes.assign(source, source + image->buffer_view->size);
                }
                else if (image->uri)
                {
                    // "data:image/png;base64,<text>"
                    const char* comma = std::strchr(image->uri, ',');
                    if (comma && std::strstr(image->uri, ";base64"))
                    {
                        const char* text = comma + 1;
                        const size_t length = std::strlen(text);
                        size_t size = length / 4 * 3;
                        if (length >= 1 && text[length - 1] == '=') --size;
                        if (length >= 2 && text[length - 2] == '=') --size;

                        cgltf_options options{};
                        void* decoded = nullptr;
                        if (cgltf_load_buffer_base64(&options, size, text, &decoded) == cgltf_result_success)
                        {
                            bytes.assign(static_cast<unsigned char*>(decoded), static_cast<unsigned char*>(decoded) + size);
                            std::free(decoded);
                        }
                    }
                }

                if (bytes.empty())
                {
                    Z_CORE_WARN("glTF '{0}': image {1} has no usable data (skipped)", context.Path, imageIndex);
                }
                else
                {
                    key = "gltf:" + context.Path + "#" + std::to_string(imageIndex);
                    context.Out->EmbeddedImages.emplace_back(key, std::move(bytes));
                }
            }

            context.ImageKeys[imageIndex] = key;
            return key;
        }

        MaterialSlot ConvertMaterial(ImportContext& context, const cgltf_material* material)
        {
            MaterialSlot slot;
            slot.Albedo = glm::vec3(0.8f);   // same grey the renderer uses for "no material"

            if (!material)
                return slot;

            slot.Name = material->name ? material->name : "";

            if (material->has_pbr_metallic_roughness)
            {
                const cgltf_pbr_metallic_roughness& pbr = material->pbr_metallic_roughness;

                slot.Albedo = glm::vec3(pbr.base_color_factor[0], pbr.base_color_factor[1], pbr.base_color_factor[2]);
                slot.Metallic = pbr.metallic_factor;
                slot.Roughness = pbr.roughness_factor;
                slot.AlbedoMap = ResolveTexture(context, pbr.base_color_texture);

                /* glTF packs roughness (green) and metallic (blue) into ONE texture. The slot has separate
                   map fields, so that texture goes in the roughness field and the flag tells the shader to
                   read both from it. In this mode the factors above multiply the texture, as the spec says. */
                const std::string packed = ResolveTexture(context, pbr.metallic_roughness_texture);
                if (!packed.empty())
                {
                    slot.RoughnessMap = packed;
                    slot.PackedMetallicRoughness = true;
                }
            }

            slot.NormalMap = ResolveTexture(context, material->normal_texture);
            slot.AOMap = ResolveTexture(context, material->occlusion_texture);
            return slot;
        }

        /* Adds one glTF primitive's triangles (and its material) to the result. restMatrix is the rest-pose
           matrix of the node that holds the mesh; animatedNodeIndex is that node if an animation moves it, else -1. */
        void ReadPrimitive(ImportContext& context, const cgltf_primitive& primitive,
                           const glm::mat4& restMatrix, int animatedNodeIndex, int skinIndex,
                           std::unordered_map<const cgltf_material*, MaterialSlot>& materialCache)
        {
            GLTFImportResult& out = *context.Out;

            if (primitive.has_draco_mesh_compression)
            {
                Z_CORE_WARN("glTF '{0}': a Draco-compressed primitive was skipped (not supported)", context.Path);
                return;
            }
            if (primitive.type != cgltf_primitive_type_triangles
                && primitive.type != cgltf_primitive_type_triangle_strip
                && primitive.type != cgltf_primitive_type_triangle_fan)
            {
                return;   // points and lines
            }

            const cgltf_accessor* positions = cgltf_find_accessor(&primitive, cgltf_attribute_type_position, 0);
            if (!positions || positions->count == 0)
                return;

            const cgltf_accessor* normals = cgltf_find_accessor(&primitive, cgltf_attribute_type_normal, 0);
            const cgltf_accessor* texCoords = cgltf_find_accessor(&primitive, cgltf_attribute_type_texcoord, 0);
            const cgltf_accessor* joints = cgltf_find_accessor(&primitive, cgltf_attribute_type_joints, 0);
            const cgltf_accessor* weights = cgltf_find_accessor(&primitive, cgltf_attribute_type_weights, 0);

            // Skinned only if the node has a skin AND this primitive carries the weights for it.
            const bool skinned = skinIndex >= 0 && joints && weights;
            if (!skinned)
                skinIndex = -1;

            const size_t vertexCount = positions->count;
            const uint32_t baseVertex = static_cast<uint32_t>(out.Vertices.size());

            /* A skinned mesh's vertices stay in bind-pose space: the joints move them. Any other mesh gets
               its node's rest transform applied now, so it sits right with no work at draw time (and so
               picking, which reads these vertices, agrees with what's on screen). */
            const glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(restMatrix)));

            for (size_t v = 0; v < vertexCount; ++v)
            {
                MeshVertex vertex;
                vertex.Position = glm::vec3(0.0f);
                vertex.Normal = glm::vec3(0.0f);
                vertex.TexCoord = glm::vec2(0.0f);

                cgltf_accessor_read_float(positions, v, &vertex.Position.x, 3);
                if (normals)
                    cgltf_accessor_read_float(normals, v, &vertex.Normal.x, 3);
                if (texCoords)
                {
                    cgltf_accessor_read_float(texCoords, v, &vertex.TexCoord.x, 2);
                    /* glTF's UV origin is the top-left of the image, OBJ's is the bottom-left, and
                       OpenGLTexture2D flips every image vertically when it loads it (the OBJ convention).
                       Flipping V here keeps the two cancelling out. */
                    vertex.TexCoord.y = 1.0f - vertex.TexCoord.y;
                }

                if (!skinned)
                {
                    vertex.Position = glm::vec3(restMatrix * glm::vec4(vertex.Position, 1.0f));
                    if (normals)
                        vertex.Normal = glm::normalize(normalMatrix * vertex.Normal);
                }

                out.Vertices.push_back(vertex);

                SkinVertex skin;   // all zero for parts that aren't skinned
                if (skinned)
                {
                    cgltf_uint j[4] = { 0, 0, 0, 0 };
                    float w[4] = { 0, 0, 0, 0 };
                    cgltf_accessor_read_uint(joints, v, j, 4);
                    cgltf_accessor_read_float(weights, v, w, 4);

                    const float sum = w[0] + w[1] + w[2] + w[3];
                    if (sum > 1e-6f)
                    {
                        for (int k = 0; k < 4; ++k)
                        {
                            // A joint index the shader's array doesn't have would read garbage; use 0 with no pull.
                            const bool valid = j[k] < static_cast<cgltf_uint>(ModelData::MaxJoints);
                            skin.Joints[k] = valid ? static_cast<float>(j[k]) : 0.0f;
                            skin.Weights[k] = valid ? w[k] / sum : 0.0f;
                        }
                    }
                    else
                    {
                        skin.Weights = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);   // no weights at all: stick to joint 0
                    }
                }
                out.SkinVertices.push_back(skin);
            }

            // Indices, turned into a plain triangle list.
            const size_t sourceCount = primitive.indices ? primitive.indices->count : vertexCount;
            auto sourceIndex = [&](size_t i) -> uint32_t
            {
                return primitive.indices ? static_cast<uint32_t>(cgltf_accessor_read_index(primitive.indices, i)) : static_cast<uint32_t>(i);
            };

            std::vector<uint32_t> triangles;
            if (primitive.type == cgltf_primitive_type_triangles)
            {
                for (size_t i = 0; i + 2 < sourceCount; i += 3)
                    triangles.insert(triangles.end(), { sourceIndex(i), sourceIndex(i + 1), sourceIndex(i + 2) });
            }
            else if (primitive.type == cgltf_primitive_type_triangle_strip)
            {
                for (size_t i = 0; i + 2 < sourceCount; ++i)
                {
                    // Every other triangle of a strip is wound the other way round.
                    if (i % 2 == 0) triangles.insert(triangles.end(), { sourceIndex(i), sourceIndex(i + 1), sourceIndex(i + 2) });
                    else            triangles.insert(triangles.end(), { sourceIndex(i + 1), sourceIndex(i), sourceIndex(i + 2) });
                }
            }
            else
            {
                for (size_t i = 1; i + 1 < sourceCount; ++i)
                    triangles.insert(triangles.end(), { sourceIndex(0), sourceIndex(i), sourceIndex(i + 1) });
            }

            // A node with a negative scale (a mirror) turns every triangle inside out; swap two corners to fix it.
            if (!skinned && glm::determinant(glm::mat3(restMatrix)) < 0.0f)
                for (size_t i = 0; i + 2 < triangles.size(); i += 3)
                    std::swap(triangles[i + 1], triangles[i + 2]);

            // Drop triangles that point past the vertex list (bad file), and move the rest to their place in the shared buffer.
            const uint32_t indexOffset = static_cast<uint32_t>(out.Indices.size());
            for (size_t i = 0; i + 2 < triangles.size(); i += 3)
            {
                if (triangles[i] >= vertexCount || triangles[i + 1] >= vertexCount || triangles[i + 2] >= vertexCount)
                    continue;
                for (size_t k = 0; k < 3; ++k)
                    out.Indices.push_back(baseVertex + triangles[i + k]);
            }
            const uint32_t indexCount = static_cast<uint32_t>(out.Indices.size()) - indexOffset;
            if (indexCount == 0)
                return;

            // No normals in the file: smooth ones from the triangles (each corner gets the sum of its faces' normals).
            if (!normals)
            {
                for (uint32_t i = indexOffset; i + 2 < indexOffset + indexCount; i += 3)
                {
                    MeshVertex& a = out.Vertices[out.Indices[i]];
                    MeshVertex& b = out.Vertices[out.Indices[i + 1]];
                    MeshVertex& c = out.Vertices[out.Indices[i + 2]];
                    const glm::vec3 faceNormal = glm::cross(b.Position - a.Position, c.Position - a.Position);
                    a.Normal += faceNormal;
                    b.Normal += faceNormal;
                    c.Normal += faceNormal;
                }
                for (size_t v = baseVertex; v < out.Vertices.size(); ++v)
                {
                    const float length = glm::length(out.Vertices[v].Normal);
                    out.Vertices[v].Normal = length > 0.0f ? out.Vertices[v].Normal / length : glm::vec3(0.0f, 1.0f, 0.0f);
                }
            }

            // Materials are converted once per glTF material, then copied for each submesh that uses it.
            auto cached = materialCache.find(primitive.material);
            if (cached == materialCache.end())
                cached = materialCache.emplace(primitive.material, ConvertMaterial(context, primitive.material)).first;

            MaterialSlot slot = cached->second;
            slot.SubmeshIndex = static_cast<int>(out.Submeshes.size());
            out.Materials.push_back(slot);

            Submesh submesh;
            submesh.MaterialName = slot.Name;
            submesh.IndexOffset = indexOffset;
            submesh.IndexCount = indexCount;
            submesh.NodeIndex = skinned ? -1 : animatedNodeIndex;
            submesh.SkinIndex = skinIndex;
            out.Submeshes.push_back(submesh);
        }
    }

    bool GLTFLoader::Import(const std::string& path, GLTFImportResult& out)
    {
        cgltf_options options{};
        cgltf_data* data = nullptr;

        cgltf_result result = cgltf_parse_file(&options, path.c_str(), &data);
        if (result != cgltf_result_success)
        {
            Z_CORE_ERROR("Could not read glTF file '{0}' (cgltf error {1})", path, static_cast<int>(result));
            return false;
        }

        // Frees the parsed file on every way out of this function.
        struct Free { cgltf_data* Data; ~Free() { cgltf_free(Data); } } freeOnExit{ data };

        // Loads the .bin files / the .glb's binary chunk, which hold the actual vertex data.
        result = cgltf_load_buffers(&options, data, path.c_str());
        if (result != cgltf_result_success)
        {
            Z_CORE_ERROR("Could not load the data buffers of glTF file '{0}' (cgltf error {1}). Is the .bin next to it?", path, static_cast<int>(result));
            return false;
        }

        // Checks that every accessor stays inside its buffer, so a corrupt file can't make us read past the end.
        result = cgltf_validate(data);
        if (result != cgltf_result_success)
        {
            Z_CORE_ERROR("glTF file '{0}' failed validation (cgltf error {1})", path, static_cast<int>(result));
            return false;
        }

        ImportContext context;
        context.Data = data;
        context.Path = path;
        context.Directory = std::filesystem::path(path).parent_path().string();
        context.Out = &out;

        out.Model = std::make_shared<ModelData>();
        ReadNodes(data, *out.Model);
        ReadSkins(data, *out.Model);
        ReadAnimations(data, *out.Model);
        out.Model->Finalize();

        std::unordered_map<const cgltf_material*, MaterialSlot> materialCache;
        bool anySkinned = false;

        for (size_t n = 0; n < data->nodes_count; ++n)
        {
            const cgltf_node& node = data->nodes[n];
            if (!node.mesh)
                continue;

            const int nodeIndex = static_cast<int>(n);
            const int skinIndex = node.skin ? static_cast<int>(cgltf_skin_index(data, node.skin)) : -1;
            const glm::mat4& restMatrix = out.Model->RestPose.NodeMatrices[n];
            const int animatedNode = out.Model->IsNodeAnimated(nodeIndex) ? nodeIndex : -1;

            for (size_t p = 0; p < node.mesh->primitives_count; ++p)
            {
                const size_t before = out.Submeshes.size();
                ReadPrimitive(context, node.mesh->primitives[p], restMatrix, animatedNode, skinIndex, materialCache);
                if (out.Submeshes.size() > before && out.Submeshes.back().SkinIndex >= 0)
                    anySkinned = true;
            }
        }

        if (out.Vertices.empty() || out.Indices.empty())
        {
            Z_CORE_ERROR("glTF file '{0}' contains no triangles", path);
            return false;
        }

        // Every vertex got a SkinVertex while reading; if nothing is skinned the buffer would be all zeros, so drop it.
        if (!anySkinned)
            out.SkinVertices.clear();

        return true;
    }

    std::shared_ptr<Mesh> GLTFLoader::Load(const std::string& path)
    {
        GLTFImportResult imported;
        if (!Import(path, imported))
            return nullptr;

        // Hand the packed-in images to the texture library before anything asks for them.
        for (auto& [key, bytes] : imported.EmbeddedImages)
            TextureLibrary::RegisterEncoded(key, std::move(bytes));

        const size_t submeshCount = imported.Submeshes.size();
        const size_t clipCount = imported.Model->Animations.size();
        const size_t skinCount = imported.Model->Skins.size();

        Z_CORE_INFO("Loaded glTF '{0}': {1} vertices, {2} submeshes, {3} skins, {4} animations",
                    path, imported.Vertices.size(), submeshCount, skinCount, clipCount);

        return std::make_shared<Mesh>(std::move(imported.Vertices), std::move(imported.Indices),
                                      std::move(imported.Submeshes), std::move(imported.SkinVertices),
                                      std::move(imported.Model), std::move(imported.Materials));
    }
}
