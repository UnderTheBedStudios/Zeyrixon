#include <ZeyrixonEditor/Core/EditorLayer.h>
#include <Zeyrixon/Embedded/EmbeddedResources.h>
#include <Zeyrixon/ImGui/ImGuiLayer.h>
#include <Zeyrixon/Events/MouseEvent.h>
#include <Zeyrixon/Core/Application.h>
#include <Zeyrixon/World/ComponentRegistry.h>
#include <Zeyrixon/Renderer/WorldRenderer.h>
#include <Zeyrixon/Renderer/Picking.h>
#include <Zeyrixon/Renderer/Mesh.h>
#include <Zeyrixon/Scripting/ScriptEngine.h>
#include <Zeyrixon/Renderer/DebugRenderer.h>
#include <Zeyrixon/Physics/Collider.h>

#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glad/glad.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <tinyfiledialogs.h>

#include <filesystem>
#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <limits>

namespace fs = std::filesystem;

namespace
{
    // Splits `directory`'s immediate children into sorted dirs/files lists.
    void ListDirectoryContents(const fs::path& directory,
        std::vector<fs::directory_entry>& outDirs,
        std::vector<fs::directory_entry>& outFiles)
    {
        std::error_code ec;
        for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied, ec))
        {
            if (entry.is_directory())
                outDirs.push_back(entry);
            else
                outFiles.push_back(entry);
        }

        auto byName = [](const fs::directory_entry& a, const fs::directory_entry& b)
        {
            return a.path().filename().string() < b.path().filename().string();
        };
        std::sort(outDirs.begin(), outDirs.end(), byName);
        std::sort(outFiles.begin(), outFiles.end(), byName);
    }

    /* "Edit Collider in Viewport" in the Collider inspector. It lives here, not in EditorLayer, because the
       inspector lambdas are free functions with no access to the layer. Only one entity is selected at a
       time, so one flag is enough. While it is on, the move/rotate/scale gizmo is hidden (both want the
       mouse) and the collider's resize handles are shown instead. */
    bool g_EditCollider = false;
    int g_EditColliderSlot = 0;   // which of the collider's slots the handles resize

    // True while the viewport handles are editing a slot of THIS entity's collider (and that slot exists).
    bool EditingColliderSlot(Zeyrixon::Entity entity)
    {
        return g_EditCollider && entity && entity.HasComponent<Zeyrixon::ColliderComponent>()
            && g_EditColliderSlot >= 0
            && g_EditColliderSlot < static_cast<int>(entity.GetComponent<Zeyrixon::ColliderComponent>().Slots.size());
    }

    /* The bounding box, in the mesh's own space, of one submesh; or of the whole mesh when submeshIndex is -1
       (or isn't a submesh of this mesh). A submesh is just a range of the index buffer, so this walks that
       range and takes the extremes of the vertices it uses. For skinned glTF parts this is the rest pose. */
    void GetSubmeshBounds(const Zeyrixon::Mesh& mesh, int submeshIndex, glm::vec3& boundsMin, glm::vec3& boundsMax)
    {
        boundsMin = mesh.GetBoundsMin();
        boundsMax = mesh.GetBoundsMax();

        const auto& submeshes = mesh.GetSubmeshes();
        if (submeshIndex < 0 || submeshIndex >= static_cast<int>(submeshes.size()))
            return;

        const auto& vertices = mesh.GetVerticies();
        const auto& indices = mesh.GetIndices();
        const Zeyrixon::Submesh& submesh = submeshes[submeshIndex];

        glm::vec3 lo(std::numeric_limits<float>::max());
        glm::vec3 hi(std::numeric_limits<float>::lowest());
        bool any = false;
        for (size_t i = submesh.IndexOffset; i < static_cast<size_t>(submesh.IndexOffset) + submesh.IndexCount && i < indices.size(); ++i)
        {
            if (indices[i] >= vertices.size())
                continue;

            lo = glm::min(lo, vertices[indices[i]].Position);
            hi = glm::max(hi, vertices[indices[i]].Position);
            any = true;
        }

        if (any)   // an empty submesh keeps the whole-mesh box rather than a nonsense one
        {
            boundsMin = lo;
            boundsMax = hi;
        }
    }

    // Moves and sizes one slot so its shape wraps the given box (in the entity's own space).
    void FitSlotToBounds(Zeyrixon::ColliderSlot& slot, const glm::vec3& boundsMin, const glm::vec3& boundsMax)
    {
        constexpr float minSize = 0.01f;
        const glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
        const glm::vec3 half = (boundsMax - boundsMin) * 0.5f;

        slot.Offset = center;
        switch (slot.Shape)
        {
            case Zeyrixon::ColliderShape::Box:
                slot.HalfExtents = glm::max(half, glm::vec3(minSize));
                break;
            case Zeyrixon::ColliderShape::Sphere:
                slot.Radius = std::max({ half.x, half.y, half.z, minSize });
                break;
            case Zeyrixon::ColliderShape::Capsule:
                slot.Radius = std::max({ half.x, half.z, minSize });
                slot.Height = std::max(half.y * 2.0f, slot.Radius * 2.0f);
                break;
        }
    }

    bool DeleteKeyPressed()
    {
        return ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
            && !ImGui::GetIO().WantTextInput
            && ImGui::IsKeyPressed(ImGuiKey_Delete, false);
    }

    const char* k_LogLevelNames[] = { "Trace", "Info", "Warning", "Error", "Critical" };
    const spdlog::level::level_enum k_LogLevelValues[] = {
        spdlog::level::trace, spdlog::level::info, spdlog::level::warn,
        spdlog::level::err, spdlog::level::critical
    };

    ImVec4 LogLevelColor(spdlog::level::level_enum level)
    {
        switch (level)
        {
            case spdlog::level::trace:
            case spdlog::level::debug:    return ImVec4(0.55f, 0.55f, 0.55f, 1.0f);
            case spdlog::level::warn:     return ImVec4(1.0f, 0.85f, 0.3f, 1.0f);
            case spdlog::level::err:      return ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
            case spdlog::level::critical: return ImVec4(1.0f, 0.15f, 0.55f, 1.0f);
            default:                      return ImGui::GetStyleColorVec4(ImGuiCol_Text);
        }
    }

    void SetInspector(const char* name, std::function<void(Zeyrixon::Entity)> draw)
    {
        if (auto* info = Zeyrixon::ComponentRegistry::Get().Find(name))
            info->DrawInspector = std::move(draw);
    }

    // One row of the material inspector: an editable path (applied on Enter), Browse and Clear.
    void DrawTextureSlot(const char* label, std::string& path)
    {
        ImGui::PushID(label);

        char buffer[512]{};
        std::strncpy(buffer, path.c_str(), sizeof(buffer) - 1);
        if (ImGui::InputText(label, buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
            path = buffer;

        if (ImGui::Button("Browse..."))
        {
            const char* filterPatterns[3] = { "*.png", "*.jpg", "*.jpeg" };
            const char* chosen = tinyfd_openFileDialog("Choose a texture", nullptr, 3, filterPatterns, "Images", 0);
            if (chosen)
                path = chosen;
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear"))
            path.clear();

        ImGui::PopID();
    }

    void RegisterComponentInspectors()
    {
        SetInspector("Tag", [](Zeyrixon::Entity entity)
        {
            auto& tag = entity.GetComponent<Zeyrixon::TagComponent>().Tag;

            char buffer[256]{};
            std::strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Name", buffer, sizeof(buffer)))
                tag = buffer;
        });

        SetInspector("Transform", [](Zeyrixon::Entity entity)
        {
            auto& transform = entity.GetComponent<Zeyrixon::TransformComponent>();

            ImGui::DragFloat3("Position", &transform.Position.x, 0.1f);

            // Stored in radians, edited in degrees.
            glm::vec3 rotationDegrees = glm::degrees(transform.Rotation);
            if (ImGui::DragFloat3("Rotation", &rotationDegrees.x, 0.5f))
                transform.Rotation = glm::radians(rotationDegrees);

            ImGui::DragFloat3("Scale", &transform.Scale.x, 0.1f);
        });

        SetInspector("Camera", [](Zeyrixon::Entity entity)
        {
            auto& camera = entity.GetComponent<Zeyrixon::CameraComponent>();

            ImGui::Checkbox("Primary", &camera.Primary);
            ImGui::Checkbox("Orthographic", &camera.Orthographic);

            if (camera.Orthographic)
                ImGui::DragFloat("Size", &camera.OrthographicSize, 0.05f, 0.1f, 1000.f);
            else
                ImGui::DragFloat("FOV", &camera.FOV, 0.1f, 1.0f, 179.0f);

            ImGui::DragFloat("Near", &camera.Near, 0.01f, 0.001f, 100.0f);
            ImGui::DragFloat("Far", &camera.Far, 1.0f, 1.0f, 100000.0f);
        });

        SetInspector("Light", [](Zeyrixon::Entity entity)
        {
            auto& light = entity.GetComponent<Zeyrixon::LightComponent>();

            const char* typeNames[] = { "Directional", "Point", "Spot" };
            int type = static_cast<int>(light.Type);
            if (ImGui::Combo("Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
                light.Type = static_cast<Zeyrixon::LightType>(type);

            ImGui::ColorEdit3("Color", &light.Color.x);
            ImGui::DragFloat("Intensity", &light.Intensity, 0.1f, 0.0f, 1000.0f);

            // A directional light has no position and never fades, so range only matters for the others.
            if (light.Type != Zeyrixon::LightType::Directional)
                ImGui::DragFloat("Range", &light.Range, 0.1f, 0.01f, 10000.0f);

            if (light.Type == Zeyrixon::LightType::Spot)
            {
                // Inner can never be wider than outer, so each drag clamps against the other.
                ImGui::DragFloat("Outer Angle", &light.OuterConeAngle, 0.2f, 1.0f, 89.0f, "%.1f deg");
                if (ImGui::DragFloat("Inner Angle", &light.InnerConeAngle, 0.2f, 0.0f, 89.0f, "%.1f deg"))
                    light.InnerConeAngle = std::min(light.InnerConeAngle, light.OuterConeAngle);
                light.InnerConeAngle = std::min(light.InnerConeAngle, light.OuterConeAngle);
            }

            if (light.Type != Zeyrixon::LightType::Point)
                ImGui::TextDisabled("Shines along the entity's -Z axis (rotate it to aim).");

            if (light.Type == Zeyrixon::LightType::Directional)
            {
                ImGui::SeparatorText("Shadows");
                ImGui::Checkbox("Cast Shadows", &light.CastShadows);
                if (light.CastShadows)
                {
                    ImGui::DragFloat("Shadow Distance", &light.ShadowDistance, 0.5f, 1.0f, 1000.0f);
                    ImGui::DragFloat("Depth Bias", &light.ShadowBias, 0.005f, 0.0f, 2.0f, "%.3f");
                    ImGui::DragFloat("Normal Bias", &light.ShadowNormalBias, 0.05f, 0.0f, 10.0f, "%.2f");
                    ImGui::TextDisabled("Only the first shadow-casting directional light is used.");
                }
            }
            else
                ImGui::TextDisabled("Shadows: directional lights only for now.");
        });

        SetInspector("Collider", [](Zeyrixon::Entity entity)
        {
            auto& collider = entity.GetComponent<Zeyrixon::ColliderComponent>();

            // The mesh, if the entity has one: "Fit to Mesh" and "One Slot per Submesh" read its bounds.
            std::shared_ptr<Zeyrixon::Mesh> mesh;
            if (entity.HasComponent<Zeyrixon::MeshComponent>())
                mesh = Zeyrixon::MeshLibrary::Get(entity.GetComponent<Zeyrixon::MeshComponent>().MeshPath);
            const int submeshCount = mesh ? static_cast<int>(mesh->GetSubmeshes().size()) : 0;

            constexpr float minSize = 0.01f;

            int removeIndex = -1;
            for (int i = 0; i < static_cast<int>(collider.Slots.size()); ++i)
            {
                auto& slot = collider.Slots[i];

                // Without this, every slot's "Shape" combo, "Fit to Mesh" button, ... would share one ImGui ID.
                ImGui::PushID(i);

                std::string label = "Slot " + std::to_string(i);
                if (!slot.Name.empty())
                    label += " - " + slot.Name;

                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    char name[64];
                    std::snprintf(name, sizeof(name), "%s", slot.Name.c_str());
                    if (ImGui::InputText("Name", name, sizeof(name)))
                        slot.Name = name;

                    const char* shapeNames[] = { "Box", "Sphere", "Capsule" };
                    int shape = static_cast<int>(slot.Shape);
                    if (ImGui::Combo("Shape", &shape, shapeNames, IM_ARRAYSIZE(shapeNames)))
                        slot.Shape = static_cast<Zeyrixon::ColliderShape>(shape);

                    ImGui::DragFloat3("Center", &slot.Offset.x, 0.05f);

                    switch (slot.Shape)
                    {
                        case Zeyrixon::ColliderShape::Box:
                            ImGui::DragFloat3("Half Extents", &slot.HalfExtents.x, 0.05f, minSize, 10000.0f);
                            break;

                        case Zeyrixon::ColliderShape::Sphere:
                            ImGui::DragFloat("Radius", &slot.Radius, 0.05f, minSize, 10000.0f);
                            break;

                        case Zeyrixon::ColliderShape::Capsule:
                            ImGui::DragFloat("Radius", &slot.Radius, 0.05f, minSize, 10000.0f);
                            ImGui::DragFloat("Height", &slot.Height, 0.05f, minSize, 10000.0f);
                            break;
                    }

                    // A capsule can't be shorter than the two caps stacked on each other.
                    if (slot.Shape == Zeyrixon::ColliderShape::Capsule)
                        slot.Height = std::max(slot.Height, 2.0f * slot.Radius);

                    // Which part of the mesh "Fit to Mesh" wraps. Physics itself never reads this.
                    ImGui::DragInt("Submesh Index", &slot.SubmeshIndex, 0.1f, -1, std::max(0, submeshCount - 1));
                    if (slot.SubmeshIndex < 0)
                        ImGui::TextDisabled("Fit to Mesh wraps the whole mesh");
                    else if (slot.SubmeshIndex < submeshCount)
                        ImGui::TextDisabled("Fit to Mesh wraps: %s", mesh->GetSubmeshes()[slot.SubmeshIndex].MaterialName.empty()
                                            ? "(unnamed)" : mesh->GetSubmeshes()[slot.SubmeshIndex].MaterialName.c_str());

                    if (mesh && ImGui::Button("Fit to Mesh"))
                    {
                        glm::vec3 boundsMin, boundsMax;
                        GetSubmeshBounds(*mesh, slot.SubmeshIndex, boundsMin, boundsMax);
                        FitSlotToBounds(slot, boundsMin, boundsMax);
                    }

                    // Only one slot at a time has handles, so ticking this on one slot moves them from another.
                    bool editing = g_EditCollider && g_EditColliderSlot == i;
                    if (ImGui::Checkbox("Edit in Viewport", &editing))
                    {
                        g_EditCollider = editing;
                        g_EditColliderSlot = i;
                    }

                    if (collider.Slots.size() > 1 && ImGui::Button("Remove Slot"))
                        removeIndex = i;   // erased after the loop, so the vector isn't changed mid-iteration

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            if (removeIndex >= 0)
            {
                collider.Slots.erase(collider.Slots.begin() + removeIndex);

                // Keep the viewport handles on the same shape, or stop if that shape is the one that went.
                if (g_EditColliderSlot == removeIndex)
                    g_EditCollider = false;
                else if (g_EditColliderSlot > removeIndex)
                    --g_EditColliderSlot;
            }

            ImGui::Separator();
            if (ImGui::Button("Add Slot"))
            {
                Zeyrixon::ColliderSlot slot;
                slot.SubmeshIndex = static_cast<int>(collider.Slots.size()) < submeshCount
                    ? static_cast<int>(collider.Slots.size()) : -1;
                collider.Slots.push_back(slot);
            }

            // The collider equivalent of "Load from .mtl": one box around each part of the mesh.
            if (submeshCount > 1)
            {
                ImGui::SameLine();
                if (ImGui::Button("One Slot per Submesh"))
                {
                    collider.Slots.clear();
                    for (int i = 0; i < submeshCount; ++i)
                    {
                        Zeyrixon::ColliderSlot slot;
                        slot.Name = mesh->GetSubmeshes()[i].MaterialName;
                        slot.SubmeshIndex = i;

                        glm::vec3 boundsMin, boundsMax;
                        GetSubmeshBounds(*mesh, i, boundsMin, boundsMax);
                        FitSlotToBounds(slot, boundsMin, boundsMax);
                        collider.Slots.push_back(slot);
                    }
                    g_EditCollider = false;   // the slot the handles pointed at is gone
                }
            }

            // Sizes are in the entity's own space; the entity's Scale is applied on top.
            ImGui::TextDisabled("Sizes are multiplied by the entity's Scale. Several slots move as one body.");
        });

        SetInspector("RigidBody", [](Zeyrixon::Entity entity)
        {
            auto& body = entity.GetComponent<Zeyrixon::RigidBodyComponent>();

            const char* typeNames[] = { "Static", "Dynamic", "Kinematic" };
            int type = static_cast<int>(body.Type);
            if (ImGui::Combo("Type", &type, typeNames, IM_ARRAYSIZE(typeNames)))
                body.Type = static_cast<Zeyrixon::BodyType>(type);

            if (body.Type == Zeyrixon::BodyType::Dynamic)
            {
                ImGui::DragFloat("Mass", &body.Mass, 0.05f, 0.001f, 100000.0f);
                ImGui::SliderFloat("Friction", &body.Friction, 0.0f, 1.0f);
                ImGui::SliderFloat("Bounciness", &body.Restitution, 0.0f, 1.0f);
                ImGui::SliderFloat("Linear Damping", &body.LinearDamping, 0.0f, 1.0f);
                ImGui::SliderFloat("Angular Damping", &body.AngularDamping, 0.0f, 1.0f);
                ImGui::Checkbox("Use Gravity", &body.UseGravity);
                ImGui::Checkbox("Freeze Rotation", &body.FixedRotation);
            }
            else if (body.Type == Zeyrixon::BodyType::Kinematic)
                ImGui::TextDisabled("Moved by scripts / animation; pushes dynamic bodies.");
            else
                ImGui::TextDisabled("Never moves.");

            if (!entity.HasComponent<Zeyrixon::ColliderComponent>())
                ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "Needs a Collider component to do anything.");
        });

        SetInspector("Mesh", [](Zeyrixon::Entity entity)
        {
            auto& mesh = entity.GetComponent<Zeyrixon::MeshComponent>();

            char buffer[512]{};
            std::strncpy(buffer, mesh.MeshPath.c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText("Path", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue))
                mesh.MeshPath = buffer;

            if (ImGui::Button("Cube"))
                mesh.MeshPath = "builtin:cube";

            ImGui::SameLine();
            if (ImGui::Button("Browse..."))
            {
                const char* filterPatterns[3] = { "*.obj", "*.gltf", "*.glb" };
                const char* chosen = tinyfd_openFileDialog("Choose a mesh", nullptr, 3, filterPatterns, "3D models (OBJ, glTF)", 0);
                if (chosen)
                    mesh.MeshPath = chosen;
            }
        });

        SetInspector("Animation", [](Zeyrixon::Entity entity)
        {
            auto& animation = entity.GetComponent<Zeyrixon::AnimationComponent>();

            // The clips come from the mesh on the same entity; only glTF files carry any.
            std::shared_ptr<Zeyrixon::Mesh> mesh;
            if (entity.HasComponent<Zeyrixon::MeshComponent>())
                mesh = Zeyrixon::MeshLibrary::Get(entity.GetComponent<Zeyrixon::MeshComponent>().MeshPath);

            const Zeyrixon::ModelData* model = mesh ? mesh->GetModel().get() : nullptr;
            if (!model || model->Animations.empty())
            {
                ImGui::TextDisabled("Needs a Mesh component with a .gltf / .glb that has animations.");
                return;
            }

            const int clipCount = static_cast<int>(model->Animations.size());
            auto clipLabel = [&](int index)
            {
                return model->Animations[index].Name.empty() ? "Animation " + std::to_string(index) : model->Animations[index].Name;
            };

            const bool validClip = animation.ClipIndex >= 0 && animation.ClipIndex < clipCount;
            if (ImGui::BeginCombo("Clip", validClip ? clipLabel(animation.ClipIndex).c_str() : "(none - rest pose)"))
            {
                if (ImGui::Selectable("(none - rest pose)", !validClip))
                    animation.ClipIndex = -1;

                for (int i = 0; i < clipCount; ++i)
                {
                    ImGui::PushID(i);
                    if (ImGui::Selectable(clipLabel(i).c_str(), animation.ClipIndex == i))
                        animation.ClipIndex = i;
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }

            ImGui::DragFloat("Speed", &animation.Speed, 0.01f, -10.0f, 10.0f);
            ImGui::Checkbox("Loop", &animation.Loop);
            ImGui::SameLine();
            ImGui::Checkbox("Play On Start", &animation.PlayOnStart);

            // Drag to preview any moment of the clip without pressing Play.
            if (validClip)
            {
                const float duration = model->Animations[animation.ClipIndex].Duration;
                ImGui::SliderFloat("Time", &animation.Time, 0.0f, duration > 0.0f ? duration : 1.0f, "%.2f s");
                ImGui::TextDisabled("Length: %.2f s", duration);
            }
        });

        SetInspector("Sprite", [](Zeyrixon::Entity entity)
        {
            auto& sprite = entity.GetComponent<Zeyrixon::SpriteComponent>();

            DrawTextureSlot("Texture", sprite.TexturePath);

            ImGui::ColorEdit4("Color", &sprite.Color.x);
            ImGui::DragFloat("Pixels Per Unit", &sprite.PixelsPerUnit, 1.0f, 1.0f, 10000.0f);
            ImGui::DragInt("Sorting Order", &sprite.SortingOrder);

            ImGui::Checkbox("Flip X", &sprite.FlipX);
            ImGui::SameLine();
            ImGui::Checkbox("Flip Y", &sprite.FlipY);
            ImGui::SameLine();
            ImGui::Checkbox("Pixel Art", &sprite.PixelArt);
        });

        SetInspector("Script", [](Zeyrixon::Entity entity)
        {
            auto& script = entity.GetComponent<Zeyrixon::ScriptComponent>();

            char pathBuffer[512]{};
            std::strncpy(pathBuffer, script.ScriptPath.c_str(), sizeof(pathBuffer) - 1);
            if (ImGui::InputText("Path", pathBuffer, sizeof(pathBuffer)))
                script.ScriptPath = pathBuffer;

            if (ImGui::Button("Browse..."))
            {
                const char* filterPatterns[1] = { "*.as" };
                const char* chosen = tinyfd_openFileDialog("Choose a script", nullptr, 1, filterPatterns, "AngelScript", 0);
                if (chosen)
                {
                    // Store it relative to the project's Assets folder when it is inside it,
                    // so the project still works if the folder is moved.
                    namespace fs = std::filesystem;
                    std::error_code ec;
                    fs::path relative = fs::relative(chosen, Zeyrixon::ScriptEngine::GetAssetRoot(), ec);
                    const bool inside = !ec && !relative.empty() && *relative.begin() != "..";
                    script.ScriptPath = inside ? relative.generic_string() : std::string(chosen);
                }
            }

            ImGui::SameLine();
            if (ImGui::Button("Clear"))
                script.ScriptPath.clear();

            char classBuffer[128]{};
            std::strncpy(classBuffer, script.ClassName.c_str(), sizeof(classBuffer) - 1);
            if (ImGui::InputText("Class", classBuffer, sizeof(classBuffer)))
                script.ClassName = classBuffer;

            ImGui::TextDisabled("Class empty = same name as the file.");
        });

        SetInspector("Material", [](Zeyrixon::Entity entity)
        {
            auto& material = entity.GetComponent<Zeyrixon::MaterialComponent>();

            // The names of the mesh's submeshes (index = position): shown as a hint, and
            // matched against by "Load from .mtl".
            std::vector<std::string> submeshNames;
            if (entity.HasComponent<Zeyrixon::MeshComponent>())
            {
                const auto& meshComponent = entity.GetComponent<Zeyrixon::MeshComponent>();
                if (auto mesh = Zeyrixon::MeshLibrary::Get(meshComponent.MeshPath))
                    for (const auto& submesh : mesh->GetSubmeshes())
                        submeshNames.push_back(submesh.MaterialName);
            }

            int removeIndex = -1;
            for (int i = 0; i < static_cast<int>(material.Slots.size()); ++i)
            {
                auto& slot = material.Slots[i];

                // ImGui builds widget IDs from labels plus the ID stack. Without this, every
                // slot's "Browse..." and "Clear" buttons would share IDs with the other slots'.
                ImGui::PushID(i);

                std::string label = "Slot " + std::to_string(i);
                if (!slot.Name.empty())
                    label += " - " + slot.Name;

                if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen))
                {
                    ImGui::DragInt("Submesh Index", &slot.SubmeshIndex, 0.1f, -1,
                                   std::max(0, static_cast<int>(submeshNames.size()) - 1));

                    if (slot.SubmeshIndex < 0)
                        ImGui::TextDisabled("Affects every submesh without its own slot");
                    else if (slot.SubmeshIndex < static_cast<int>(submeshNames.size()))
                        ImGui::TextDisabled("Affects: %s", submeshNames[slot.SubmeshIndex].empty()
                                            ? "(unnamed)" : submeshNames[slot.SubmeshIndex].c_str());
                    else
                        ImGui::TextDisabled("The mesh has no submesh with this index");

                    ImGui::ColorEdit3("Albedo", &slot.Albedo.x);
                    ImGui::SliderFloat("Metallic", &slot.Metallic, 0.0f, 1.0f);
                    ImGui::SliderFloat("Roughness", &slot.Roughness, 0.0f, 1.0f);
                    ImGui::SliderFloat("AO", &slot.AO, 0.0f, 1.0f);

                    ImGui::SeparatorText("Maps (replace the values above)");
                    DrawTextureSlot("Albedo Map", slot.AlbedoMap);
                    DrawTextureSlot("Normal Map", slot.NormalMap);
                    DrawTextureSlot("Roughness Map", slot.RoughnessMap);
                    DrawTextureSlot("Metallic Map", slot.MetallicMap);
                    DrawTextureSlot("AO Map", slot.AOMap);
                    ImGui::Checkbox("Roughness map is packed (glTF)", &slot.PackedMetallicRoughness);
                    if (slot.PackedMetallicRoughness)
                        ImGui::TextDisabled("Roughness = G, Metallic = B of the Roughness Map; the sliders scale them. Metallic Map is ignored.");

                    if (material.Slots.size() > 1 && ImGui::Button("Remove Slot"))
                        removeIndex = i;   // erased after the loop, so the vector isn't changed mid-iteration

                    ImGui::TreePop();
                }

                ImGui::PopID();
            }

            if (removeIndex >= 0)
                material.Slots.erase(material.Slots.begin() + removeIndex);

            ImGui::Separator();
            if (ImGui::Button("Add Slot"))
            {
                Zeyrixon::MaterialSlot slot;
                slot.SubmeshIndex = material.Slots.size() < submeshNames.size()
                    ? static_cast<int>(material.Slots.size()) : -1;
                material.Slots.push_back(slot);
            }

            ImGui::SameLine();
            if (ImGui::Button("Load from .mtl..."))
            {
                const char* filterPatterns[1] = { "*.mtl" };
                const char* chosen = tinyfd_openFileDialog("Choose a material file", nullptr, 1, filterPatterns, "MTL file", 0);
                if (chosen)
                    material.ApplyMTL(chosen, submeshNames);
            }

            // glTF files carry their own materials: copy them into editable slots.
            ImGui::SameLine();
            if (ImGui::Button("Import from model"))
            {
                if (entity.HasComponent<Zeyrixon::MeshComponent>())
                {
                    auto mesh = Zeyrixon::MeshLibrary::Get(entity.GetComponent<Zeyrixon::MeshComponent>().MeshPath);
                    if (mesh && !mesh->GetImportedMaterials().empty())
                        material.Slots = mesh->GetImportedMaterials();
                }
            }
        });
    }
}

namespace Editor
{
    EditorLayer::EditorLayer(const std::string& startupProjectPath)
        : Layer("EditorLayer"), m_StartupProjectPath(startupProjectPath)
    {
        Zeyrixon::FramebufferSpecification spec;
        spec.Width = 1280;
        spec.Height = 720;
        m_Framebuffer = std::make_shared<Zeyrixon::OpenGLFramebuffer>(spec);

        // Content Browser / Assets panel icons, compiled into the binary.
        namespace E = Zeyrixon::Embedded;
        m_FolderClosedIcon = std::make_shared<Zeyrixon::OpenGLTexture2D>(E::FolderClosedPng, E::FolderClosedPngSize);
        m_FolderOpenIcon   = std::make_shared<Zeyrixon::OpenGLTexture2D>(E::FolderOpenPng, E::FolderOpenPngSize);
        m_FileIcon         = std::make_shared<Zeyrixon::OpenGLTexture2D>(E::FilePng, E::FilePngSize);

        // Light gizmos drawn over each Light entity in the viewport.
        m_DirectionalLightIcon = std::make_shared<Zeyrixon::OpenGLTexture2D>(E::DirectionalLightPng, E::DirectionalLightPngSize);
        m_PointLightIcon       = std::make_shared<Zeyrixon::OpenGLTexture2D>(E::PointLightPng, E::PointLightPngSize);
        m_SpotLightIcon        = std::make_shared<Zeyrixon::OpenGLTexture2D>(E::SpotLightPng, E::SpotLightPngSize);
    }

    EditorLayer::~EditorLayer()
    {
    }

    void EditorLayer::DrawGizmo()
    {
        if (m_ShowGameView)
            return;

        const glm::mat4 view = m_EditorCamera.GetView();
        const glm::mat4 projection = m_EditorCamera.GetProjection();

        ImGuizmo::SetOrthographic(m_EditorCamera.IsOrthographic());
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(m_ViewportMin.x, m_ViewportMin.y, m_ViewportSize.x, m_ViewportSize.y);

        // While the collider's own resize handles are showing, the transform gizmo would fight them for the mouse.
        const bool editingCollider = EditingColliderSlot(m_SelectedEntity);

        if (!m_SelectedEntity || m_GizmoOperation == 0 || editingCollider
            || !m_SelectedEntity.HasComponent<Zeyrixon::TransformComponent>())
            return;

        auto& transform = m_SelectedEntity.GetComponent<Zeyrixon::TransformComponent>();
        glm::vec3 rotationDegrees = glm::degrees(transform.Rotation);

        float matrix[16];
        ImGuizmo::RecomposeMatrixFromComponents(
            glm::value_ptr(transform.Position), glm::value_ptr(rotationDegrees),
            glm::value_ptr(transform.Scale), matrix
        );

        ImGuizmo::Manipulate(
            glm::value_ptr(view), glm::value_ptr(projection),
            static_cast<ImGuizmo::OPERATION>(m_GizmoOperation), ImGuizmo::WORLD, matrix
        );

        if (ImGuizmo::IsUsing())
        {
            glm::vec3 position, rotation, scale;
            ImGuizmo::DecomposeMatrixToComponents(
                matrix, glm::value_ptr(position), glm::value_ptr(rotation), glm::value_ptr(scale)
            );

            transform.Position = position;
            transform.Rotation = glm::radians(rotation);
            transform.Scale = scale;
        }
    }

    void EditorLayer::DrawColliderGizmo()
    {
        m_HoveredColliderHandle = -1;

        const bool canEdit = !m_ShowGameView && m_ActiveWorld && EditingColliderSlot(m_SelectedEntity)
            && m_SelectedEntity.HasComponent<Zeyrixon::TransformComponent>();
        if (!canEdit)
        {
            m_ActiveColliderHandle = -1;
            return;
        }

        auto& transform = m_SelectedEntity.GetComponent<Zeyrixon::TransformComponent>();

        // From here on `collider` is the ONE slot the handles resize (the rest of this function only ever
        // deals with a single shape).
        auto& collider = m_SelectedEntity.GetComponent<Zeyrixon::ColliderComponent>().Slots[g_EditColliderSlot];

        // The shape exactly as the physics engine sees it, and its local axes as they point in the world.
        const Zeyrixon::Physics::ColliderShapeWorld shape = Zeyrixon::Physics::ComputeWorldShape(collider, transform);
        const glm::mat3 axes = glm::mat3_cast(shape.Rotation);   // column i = local axis i in world space

        /* One handle on each side of each axis. A box and a sphere get all six; a capsule's X and Z handles
           set the radius and its Y handles set the height. `Distance` is how far the handle sits from the
           centre, in world units. */
        struct Handle { int Id; int Axis; float Sign; float Distance; };
        Handle handles[6];
        int handleCount = 0;
        auto addPair = [&](int axis, float distance)
        {
            handles[handleCount++] = { axis * 2,     axis,  1.0f, distance };
            handles[handleCount++] = { axis * 2 + 1, axis, -1.0f, distance };
        };

        switch (collider.Shape)
        {
            case Zeyrixon::ColliderShape::Box:
                addPair(0, shape.HalfExtents.x);
                addPair(1, shape.HalfExtents.y);
                addPair(2, shape.HalfExtents.z);
                break;
            case Zeyrixon::ColliderShape::Sphere:
                addPair(0, shape.Radius);
                addPair(1, shape.Radius);
                addPair(2, shape.Radius);
                break;
            case Zeyrixon::ColliderShape::Capsule:
                addPair(0, shape.Radius);
                addPair(1, shape.HalfHeight);
                addPair(2, shape.Radius);
                break;
        }

        const glm::mat4 viewProjection = m_EditorCamera.GetProjection() * m_EditorCamera.GetView();

        // World position -> pixel position on screen. False when the point is behind the camera.
        auto project = [&](const glm::vec3& world, ImVec2& screen) -> bool
        {
            const glm::vec4 clip = viewProjection * glm::vec4(world, 1.0f);
            if (clip.w <= 0.0001f)
                return false;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            screen = ImVec2(m_ViewportMin.x + (ndc.x * 0.5f + 0.5f) * m_ViewportSize.x,
                            m_ViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * m_ViewportSize.y);
            return true;
        };

        const ImVec2 mouse = ImGui::GetMousePos();
        glm::vec3 rayOrigin, rayDirection;
        m_EditorCamera.ScreenPointToRay(glm::vec2(mouse.x - m_ViewportMin.x, mouse.y - m_ViewportMin.y),
                                        glm::vec2(m_ViewportSize.x, m_ViewportSize.y), rayOrigin, rayDirection);

        /* Where along a handle's axis (measured from the collider's centre) is the mouse pointing?
           A mouse ray and an axis are two lines in 3D that almost never touch, so we take the point on the
           axis that is CLOSEST to the ray. With the axis written P + s*A and the ray O + t*D (A and D unit
           length), minimising the distance between them gives
               s = (b*f - c) / (1 - b*b)     where  b = A.D,  c = A.(P-O),  f = D.(P-O)
           When the ray is parallel to the axis (1 - b*b ~ 0) there is no single closest point, so we report
           failure and the handle simply doesn't move that frame. */
        auto distanceAlongAxis = [&](const glm::vec3& axis, float& out) -> bool
        {
            const glm::vec3 toCenter = shape.Center - rayOrigin;
            const float b = glm::dot(axis, rayDirection);
            const float c = glm::dot(axis, toCenter);
            const float f = glm::dot(rayDirection, toCenter);
            const float denominator = 1.0f - b * b;
            if (denominator < 1e-4f)
                return false;

            out = (b * f - c) / denominator;
            return true;
        };

        constexpr float pickRadius = 10.0f;   // pixels around a handle in which the mouse counts as on it
        const bool altHeld = Zeyrixon::Input::IsKeyPressed(Z_KEY_LEFT_ALT);   // Alt + drag orbits the camera

        if (m_ActiveColliderHandle < 0)
        {
            // Not dragging: find the handle nearest the mouse, if it is close enough.
            if (m_ViewportHovered && !altHeld)
            {
                float closest = pickRadius;
                for (int i = 0; i < handleCount; ++i)
                {
                    ImVec2 screen;
                    if (!project(shape.Center + axes[handles[i].Axis] * handles[i].Sign * handles[i].Distance, screen))
                        continue;

                    const float dx = mouse.x - screen.x;
                    const float dy = mouse.y - screen.y;
                    const float distance = std::sqrt(dx * dx + dy * dy);
                    if (distance < closest)
                    {
                        closest = distance;
                        m_HoveredColliderHandle = handles[i].Id;
                    }
                }
            }

            if (m_HoveredColliderHandle >= 0 && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_ActiveColliderHandle = m_HoveredColliderHandle;

                // Remember where on the handle we grabbed it; otherwise it would snap its centre to the cursor.
                m_ColliderGrabOffset = 0.0f;
                for (int i = 0; i < handleCount; ++i)
                {
                    float grabbed;
                    if (handles[i].Id == m_ActiveColliderHandle
                        && distanceAlongAxis(axes[handles[i].Axis] * handles[i].Sign, grabbed))
                        m_ColliderGrabOffset = grabbed - handles[i].Distance;
                }
            }
        }
        else if (ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            // Dragging: turn "the mouse is this far along the axis" back into the collider's own units.
            for (int i = 0; i < handleCount; ++i)
            {
                const Handle& handle = handles[i];
                float along;
                if (handle.Id != m_ActiveColliderHandle
                    || !distanceAlongAxis(axes[handle.Axis] * handle.Sign, along))
                    continue;

                const float wanted = std::max(along - m_ColliderGrabOffset, 0.01f);   // world units from the centre

                // The same clamp ComputeWorldShape uses; dividing by it undoes the entity's Scale.
                const glm::vec3 scale = glm::max(glm::abs(transform.Scale), glm::vec3(0.001f));

                switch (collider.Shape)
                {
                    case Zeyrixon::ColliderShape::Box:
                        collider.HalfExtents[handle.Axis] = wanted / scale[handle.Axis];
                        break;

                    case Zeyrixon::ColliderShape::Sphere:
                        collider.Radius = wanted / std::max({ scale.x, scale.y, scale.z });
                        break;

                    case Zeyrixon::ColliderShape::Capsule:
                        if (handle.Axis == 1)
                            collider.Height = std::max(2.0f * wanted / scale.y, 2.0f * collider.Radius);
                        else
                        {
                            collider.Radius = wanted / std::max(scale.x, scale.z);
                            collider.Height = std::max(collider.Height, 2.0f * collider.Radius);   // keep the caps from overlapping
                        }
                        break;
                }
            }
        }
        else
        {
            m_ActiveColliderHandle = -1;   // mouse released
        }

        // Draw the handles last, so they show the size the drag just produced.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        for (int i = 0; i < handleCount; ++i)
        {
            const Handle& handle = handles[i];

            // Recompute from the (possibly just changed) collider, not from the values gathered above.
            const Zeyrixon::Physics::ColliderShapeWorld current = Zeyrixon::Physics::ComputeWorldShape(collider, transform);
            float distance = 0.0f;
            switch (collider.Shape)
            {
                case Zeyrixon::ColliderShape::Box:     distance = current.HalfExtents[handle.Axis]; break;
                case Zeyrixon::ColliderShape::Sphere:  distance = current.Radius; break;
                case Zeyrixon::ColliderShape::Capsule: distance = handle.Axis == 1 ? current.HalfHeight : current.Radius; break;
            }

            ImVec2 screen;
            if (!project(current.Center + glm::mat3_cast(current.Rotation)[handle.Axis] * handle.Sign * distance, screen))
                continue;

            const bool hot = handle.Id == m_ActiveColliderHandle || handle.Id == m_HoveredColliderHandle;
            const float radius = hot ? 7.0f : 5.0f;
            drawList->AddCircleFilled(screen, radius, hot ? IM_COL32(255, 200, 60, 255) : IM_COL32(255, 255, 255, 235));
            drawList->AddCircle(screen, radius, IM_COL32(20, 20, 20, 255), 0, 1.5f);
        }

        if (m_ActiveColliderHandle >= 0)
        {
            // A readout next to the cursor while dragging, in the collider's own units.
            char text[96];
            switch (collider.Shape)
            {
                case Zeyrixon::ColliderShape::Box:
                    std::snprintf(text, sizeof(text), "Half extents %.2f  %.2f  %.2f",
                                  collider.HalfExtents.x, collider.HalfExtents.y, collider.HalfExtents.z);
                    break;
                case Zeyrixon::ColliderShape::Sphere:
                    std::snprintf(text, sizeof(text), "Radius %.2f", collider.Radius);
                    break;
                default:
                    std::snprintf(text, sizeof(text), "Radius %.2f  Height %.2f", collider.Radius, collider.Height);
                    break;
            }
            drawList->AddText(ImVec2(mouse.x + 14.0f, mouse.y + 10.0f), IM_COL32(255, 255, 255, 255), text);
        }
    }

    void EditorLayer::DrawLightGizmos()
    {
        m_HoveredLightGizmo = Zeyrixon::Entity{};

        // Lights don't affect sprites, so there is nothing to show in the 2D view or the game view.
        if (m_ShowGameView || m_EditorCamera.Is2D() || !m_ActiveWorld)
            return;

        const glm::mat4 viewProjection = m_EditorCamera.GetProjection() * m_EditorCamera.GetView();

        // World position -> pixel position on screen. False when the point is behind the camera.
        auto project = [&](const glm::vec3& world, ImVec2& screen) -> bool
        {
            const glm::vec4 clip = viewProjection * glm::vec4(world, 1.0f);
            if (clip.w <= 0.0001f)
                return false;

            const glm::vec3 ndc = glm::vec3(clip) / clip.w;
            if (ndc.z < -1.0f || ndc.z > 1.0f)
                return false;

            screen = ImVec2(m_ViewportMin.x + (ndc.x * 0.5f + 0.5f) * m_ViewportSize.x,
                            m_ViewportMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * m_ViewportSize.y);
            return true;
        };

        // A line between two world points; skipped if either end can't be projected.
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        auto line = [&](const glm::vec3& a, const glm::vec3& b, ImU32 color, float thickness)
        {
            ImVec2 sa, sb;
            if (project(a, sa) && project(b, sb))
                drawList->AddLine(sa, sb, color, thickness);
        };

        // A circle lying in the plane spanned by `axisA` and `axisB`, drawn as a ring of short lines.
        auto circle = [&](const glm::vec3& center, const glm::vec3& axisA, const glm::vec3& axisB,
                          float radius, ImU32 color, float thickness)
        {
            constexpr int segments = 48;
            glm::vec3 previous = center + axisA * radius;
            for (int i = 1; i <= segments; ++i)
            {
                const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
                const glm::vec3 point = center + (axisA * std::cos(angle) + axisB * std::sin(angle)) * radius;
                line(previous, point, color, thickness);
                previous = point;
            }
        };

        /* Is something solid between the camera and this point? The scene's depth buffer still holds this
           frame's picture, so read the depth under the point's pixel and compare it to the point's own
           distance. Both are measured along the camera's view direction, so this works for perspective and
           orthographic views alike. One-pixel read per light per frame is cheap. */
        const glm::mat4 view = m_EditorCamera.GetView();
        const glm::mat4 inverseViewProjection = glm::inverse(viewProjection);
        auto isOccluded = [&](const glm::vec3& world, const ImVec2& screen) -> bool
        {
            const int pixelX = static_cast<int>(screen.x - m_ViewportMin.x);
            const int pixelY = static_cast<int>(m_ViewportSize.y - (screen.y - m_ViewportMin.y));   // GL's origin is bottom-left
            if (pixelX < 0 || pixelY < 0 || pixelX >= static_cast<int>(m_ViewportSize.x) || pixelY >= static_cast<int>(m_ViewportSize.y))
                return false;

            GLint previousReadFramebuffer = 0;
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_Framebuffer->GetRendererID());

            float storedDepth = 1.0f;   // 1 = nothing drawn there
            glReadPixels(pixelX, pixelY, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &storedDepth);

            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));

            // Stored depth is 0..1; undo the projection to find where that surface is in the world.
            const glm::vec2 ndc((screen.x - m_ViewportMin.x) / m_ViewportSize.x * 2.0f - 1.0f,
                                1.0f - (screen.y - m_ViewportMin.y) / m_ViewportSize.y * 2.0f);
            glm::vec4 surface = inverseViewProjection * glm::vec4(ndc, storedDepth * 2.0f - 1.0f, 1.0f);
            surface /= surface.w;

            const float surfaceDistance = -(view * surface).z;
            const float lightDistance = -(view * glm::vec4(world, 1.0f)).z;

            // A little slack so a light sitting right on a surface doesn't count as hidden by it.
            return surfaceDistance < lightDistance - std::max(0.1f, lightDistance * 0.01f);
        };

        const ImVec2 mouse = ImGui::GetMousePos();
        const bool mouseInViewport = mouse.x >= m_ViewportMin.x && mouse.x <= m_ViewportMin.x + m_ViewportSize.x
                                  && mouse.y >= m_ViewportMin.y && mouse.y <= m_ViewportMin.y + m_ViewportSize.y;

        constexpr float iconSize = 32.0f;   // pixels; the same size at any distance, like most editors
        constexpr float half = iconSize * 0.5f;
        float closestMouseDistance = half;  // must be inside the icon to count as hovered

        auto lights = m_ActiveWorld->GetRegistry().view<Zeyrixon::TransformComponent, Zeyrixon::LightComponent>();
        for (auto [handle, transform, light] : lights.each())
        {
            ImVec2 center;
            if (!project(transform.Position, center))
                continue;

            Zeyrixon::Entity entity(handle, m_ActiveWorld.get());
            const bool selected = (entity == m_SelectedEntity);

            const std::shared_ptr<Zeyrixon::OpenGLTexture2D>& icon =
                light.Type == Zeyrixon::LightType::Directional ? m_DirectionalLightIcon
              : light.Type == Zeyrixon::LightType::Spot        ? m_SpotLightIcon
                                                               : m_PointLightIcon;
            if (!icon || !icon->IsLoaded())
                continue;

            // Hover: the nearest icon centre to the mouse, inside the icon's radius.
            if (mouseInViewport)
            {
                const float dx = mouse.x - center.x;
                const float dy = mouse.y - center.y;
                const float distance = std::sqrt(dx * dx + dy * dy);
                if (distance < closestMouseDistance)
                {
                    closestMouseDistance = distance;
                    m_HoveredLightGizmo = entity;
                }
            }
            const bool hovered = (m_HoveredLightGizmo == entity);

            // A pale disc behind the drawing keeps the black line art readable over dark parts of the scene
            // (and gives the icon a clear click target).
            // Behind a wall or other solid? Dim it (rather than hide it) so you can still find and click it.
            const float visibility = isOccluded(transform.Position, center) ? 0.35f : 1.0f;

            const float radius = half + 3.0f;
            drawList->AddCircleFilled(center, radius, IM_COL32(235, 235, 235, static_cast<int>((hovered || selected ? 235 : 170) * visibility)), 32);
            if (selected)
                drawList->AddCircle(center, radius, IM_COL32(255, 175, 40, 255), 32, 2.5f);
            else if (hovered)
                drawList->AddCircle(center, radius, IM_COL32(90, 150, 255, 255), 32, 2.0f);

            // The icons were loaded flipped (see the content browser icons), hence the swapped V coordinates.
            drawList->AddImage((void*)(intptr_t)icon->GetRendererID(),
                               ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half),
                               ImVec2(0, 1), ImVec2(1, 0), IM_COL32(255, 255, 255, static_cast<int>(255 * visibility)));

            // ---- Extra outlines for the selected light only ----
            if (!selected)
                continue;

            const ImU32 outerColor = IM_COL32(255, 200, 70, 230);
            const ImU32 innerColor = IM_COL32(255, 200, 70, 110);
            const glm::vec3 position = transform.Position;
            const glm::vec3 direction = Zeyrixon::LightComponent::GetDirection(transform);

            // Two axes at right angles to the light's direction, for drawing circles around it.
            const glm::vec3 helperUp = std::abs(direction.y) > 0.99f ? glm::vec3(0.f, 0.f, 1.f) : glm::vec3(0.f, 1.f, 0.f);
            const glm::vec3 axisA = glm::normalize(glm::cross(direction, helperUp));
            const glm::vec3 axisB = glm::cross(axisA, direction);

            switch (light.Type)
            {
                case Zeyrixon::LightType::Directional:
                {
                    // Parallel rays: a bundle of equal arrows along the direction the light travels.
                    constexpr float length = 3.0f;
                    constexpr float spread = 0.6f;
                    const glm::vec3 offsets[5] = { glm::vec3(0.f), axisA * spread, -axisA * spread, axisB * spread, -axisB * spread };
                    for (const glm::vec3& offset : offsets)
                        line(position + offset, position + offset + direction * length, outerColor, 1.5f);

                    // Arrow head on the middle ray.
                    const glm::vec3 tip = position + direction * length;
                    line(tip, tip - direction * 0.4f + axisA * 0.2f, outerColor, 1.5f);
                    line(tip, tip - direction * 0.4f - axisA * 0.2f, outerColor, 1.5f);
                    line(tip, tip - direction * 0.4f + axisB * 0.2f, outerColor, 1.5f);
                    line(tip, tip - direction * 0.4f - axisB * 0.2f, outerColor, 1.5f);
                    break;
                }

                case Zeyrixon::LightType::Point:
                {
                    // Three rings make a sphere the size of the light's range.
                    const glm::vec3 x(1.f, 0.f, 0.f), y(0.f, 1.f, 0.f), z(0.f, 0.f, 1.f);
                    circle(position, x, y, light.Range, outerColor, 1.5f);
                    circle(position, y, z, light.Range, outerColor, 1.5f);
                    circle(position, x, z, light.Range, outerColor, 1.5f);
                    break;
                }

                case Zeyrixon::LightType::Spot:
                {
                    // Same clamping the renderer applies, so the outline is what you actually get.
                    const float outer = glm::clamp(light.OuterConeAngle, 1.0f, 89.0f);
                    const float inner = glm::clamp(light.InnerConeAngle, 0.0f, outer);
                    const float range = std::max(light.Range, 0.01f);

                    // The cone's base sits where the edge of the beam meets the range.
                    auto drawCone = [&](float angleDegrees, ImU32 color, bool drawEdges)
                    {
                        const float angle = glm::radians(angleDegrees);
                        const glm::vec3 baseCenter = position + direction * (range * std::cos(angle));
                        const float baseRadius = range * std::sin(angle);
                        circle(baseCenter, axisA, axisB, baseRadius, color, 1.5f);

                        if (drawEdges)
                        {
                            line(position, baseCenter + axisA * baseRadius, color, 1.5f);
                            line(position, baseCenter - axisA * baseRadius, color, 1.5f);
                            line(position, baseCenter + axisB * baseRadius, color, 1.5f);
                            line(position, baseCenter - axisB * baseRadius, color, 1.5f);
                        }
                    };

                    drawCone(outer, outerColor, true);
                    drawCone(inner, innerColor, false);
                    break;
                }
            }
        }
    }

    void EditorLayer::DeleteEntity(Zeyrixon::Entity entity)
    {
        if (!m_ActiveWorld || !entity)
            return;

        // Clear the selection first so the properties panel and gizmo never see the deletion
        if (m_SelectedEntity == entity)
            m_SelectedEntity = {};

        m_ActiveWorld->DestroyEntity(entity);
    }

    void EditorLayer::BuildDefaultLayout(ImGuiID dockspaceId)
    {
        ImGui::DockBuilderRemoveNode(dockspaceId);
        ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

        ImGuiID dockMain = dockspaceId;
        ImGuiID dockRight = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Right, 0.20f, nullptr, &dockMain);
        ImGuiID dockRightBottom = ImGui::DockBuilderSplitNode(dockRight, ImGuiDir_Down, 0.35f, nullptr, &dockRight);

        ImGuiID dockBottom = ImGui::DockBuilderSplitNode(dockMain, ImGuiDir_Down, 0.30f, nullptr, &dockMain);
        ImGuiID dockBottomLeft = ImGui::DockBuilderSplitNode(dockBottom, ImGuiDir_Left, 0.25f, nullptr, &dockBottom);

        ImGui::DockBuilderDockWindow("Viewport", dockMain);
        ImGui::DockBuilderDockWindow("World", dockRight);
        ImGui::DockBuilderDockWindow("Properties", dockRightBottom);
        ImGui::DockBuilderDockWindow("Content Browser", dockBottomLeft);
        ImGui::DockBuilderDockWindow("Logger", dockBottom);
        ImGui::DockBuilderDockWindow("Assets", dockBottom);

        ImGui::DockBuilderFinish(dockspaceId);
    }

    void EditorLayer::OnImGuiRender()
    {
        ImGuizmo::BeginFrame();

        m_ViewportHovered = false;
        Zeyrixon::Application::Get().GetImGuiLayer()->BlockEvents(true);

        if (!m_ActiveProject)
        {
            DrawProjectLauncher();
            return;
        }

        static bool dockspaceOpen = true;

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags hostFlags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("EditorDockspaceHost", &dockspaceOpen, hostFlags);
        ImGui::PopStyleVar(3);

        ImGuiID dockspaceId = ImGui::GetID("EditorDockspace");
        if (!ImGui::DockBuilderGetNode(dockspaceId))
            BuildDefaultLayout(dockspaceId);

        ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f));

        DrawMenuBar();

        ImGui::End();

        DrawViewportPanel();
        DrawWorldOutlinerPanel();
        DrawPropertiesPanel();
        DrawLoggerPanel();
        DrawContentBrowserPanel();
        DrawAssetsPanel();
    }

    void EditorLayer::DrawProjectLauncher()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 windowSize(420.0f, 160.0f);
        ImGui::SetNextWindowPos(ImVec2(
            viewport->WorkPos.x + (viewport->WorkSize.x - windowSize.x) * 0.5f,
            viewport->WorkPos.y + (viewport->WorkSize.y - windowSize.y) * 0.5f));
        ImGui::SetNextWindowSize(windowSize);

        ImGui::Begin("Zeyrixon", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove);

        ImGui::TextDisabled("No project loaded.");
        ImGui::Spacing();

        if (ImGui::Button("New Project", ImVec2(-1, 40)))
            NewProject();

        ImGui::Spacing();

        if (ImGui::Button("Open Project", ImVec2(-1, 40)))
            OpenProject();

        ImGui::End();
    }

    void EditorLayer::NewProject()
    {
        const char* parentDir = tinyfd_selectFolderDialog("Choose a location for the new project", nullptr);
        if (!parentDir)
            return;

        const char* name = tinyfd_inputBox("New Project", "Project name:", "MyGame");
        if (!name || name[0] == '\0')
            return;

        m_ActiveProject = Zeyrixon::Project::New(parentDir, name);
        if (m_ActiveProject)
        {
            Zeyrixon::Application::Get().ChangeWindowTitle(("Zeyrixon Editor - " + m_ActiveProject->GetName()).c_str());
            m_SelectedDirectory.clear();
            LoadStartupWorld();
        }
    }

    void EditorLayer::OpenProject()
    {
        // Probe which native dialog backend tinyfd will use, without showing a dialog.
        // "tinyfd_query" as the title makes tinyfd skip rendering anything and just
        // fill tinyfd_response with the backend name it selected (e.g. "zenity", "kdialog").
        tinyfd_messageBox("tinyfd_query", "", "info", "info", 1);
        Z_EDITOR_INFO("tinyfd selected backend: {0}", tinyfd_response);

        const char* filterPatterns[1] = { "*.zeyrixon" };
        const char* path = tinyfd_openFileDialog("Open Project", nullptr, 1, filterPatterns, "Zeyrixon Project", 0);

        if (!path)
            return;

        LoadProjectFromPath(path);
    }

    void EditorLayer::LoadProjectFromPath(const std::string& manifestPath)
    {
        m_ActiveProject = Zeyrixon::Project::Load(manifestPath);
        if (m_ActiveProject)
        {
            Zeyrixon::Application::Get().ChangeWindowTitle(("Zeyrixon Editor - " + m_ActiveProject->GetName()).c_str());
            m_SelectedDirectory.clear();
            LoadStartupWorld();
        }
        else
            Z_EDITOR_ERROR("Failed to open project from: {0}", manifestPath);
    }

    void EditorLayer::OnAttach()
    {
        RegisterComponentInspectors();

        if (!m_StartupProjectPath.empty())
            LoadProjectFromPath(m_StartupProjectPath);
    }

    void EditorLayer::OnDetach() {}

    void EditorLayer::OnUpdate(Zeyrixon::DeltaTime dt)
    {
        /* In Game View the editor camera can't start a new drag, but it still finished one
           that's in progress, so the cursor always gets released. */
        m_EditorCamera.OnUpdate(dt, m_ViewportHovered && !m_ShowGameView);

        if (m_IsPlaying && m_ActiveWorld)
            m_ActiveWorld->OnUpdate(dt);

        m_Framebuffer->Bind();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glFrontFace(GL_CCW);

        if (m_ShowGameView)
        {
            // What the primary camera sees. Nothing is drawn if there isn't one.
            Zeyrixon::Entity camera = m_ActiveWorld ? m_ActiveWorld->GetPrimaryCamera() : Zeyrixon::Entity{};
            if (camera)
            {
                const auto& transform = camera.GetComponent<Zeyrixon::TransformComponent>();
                const auto& cameraData = camera.GetComponent<Zeyrixon::CameraComponent>();

                const float aspect = m_ViewportSize.y > 0.f ? m_ViewportSize.x / m_ViewportSize.y : 1.f;

                Zeyrixon::WorldRenderer::RenderWorld(
                    *m_ActiveWorld, transform.GetViewMatrix(), cameraData.GetProjection(aspect), transform.Position
                );

                Zeyrixon::WorldRenderer::RenderSprites(
                    *m_ActiveWorld, transform.GetViewMatrix(), cameraData.GetProjection(aspect)
                );
            }
        }
        else
        {
            const glm::mat4 view = m_EditorCamera.GetView();
            const glm::mat4 projection = m_EditorCamera.GetProjection();

            if (m_ActiveWorld)
            {
                Zeyrixon::WorldRenderer::RenderWorld(
                    *m_ActiveWorld, view, projection, m_EditorCamera.GetPosition()
                );
            }

            glDisable(GL_CULL_FACE);
            // The grid goes last: it's blended over the scene and needs the scene's depth.
            Zeyrixon::WorldRenderer::RenderGrid(view, projection, m_EditorCamera.GetPosition(), m_EditorCamera.Is2D());

            if (m_ActiveWorld)
                Zeyrixon::WorldRenderer::RenderSprites(*m_ActiveWorld, view, projection);

            // Physics wireframes, last so they sit on top. Colour tells the body type at a glance:
            // blue = static (no RigidBody, or Static), green = dynamic, yellow = kinematic. The selected
            // entity's collider is brighter.
            if (m_ShowColliders && m_ActiveWorld)
            {
                auto& registry = m_ActiveWorld->GetRegistry();
                for (auto [handle, transform, collider] : registry.view<Zeyrixon::TransformComponent, Zeyrixon::ColliderComponent>().each())
                {
                    glm::vec4 color(0.3f, 0.6f, 1.0f, 0.8f);
                    if (registry.all_of<Zeyrixon::RigidBodyComponent>(handle))
                    {
                        switch (registry.get<Zeyrixon::RigidBodyComponent>(handle).Type)
                        {
                            case Zeyrixon::BodyType::Dynamic:   color = glm::vec4(0.2f, 0.9f, 0.3f, 0.8f); break;
                            case Zeyrixon::BodyType::Kinematic: color = glm::vec4(1.0f, 0.85f, 0.2f, 0.8f); break;
                            default: break;
                        }
                    }

                    const bool selected = m_SelectedEntity && handle == m_SelectedEntity.Handle();

                    for (int i = 0; i < static_cast<int>(collider.Slots.size()); ++i)
                    {
                        glm::vec4 slotColor = color;
                        if (selected)
                        {
                            // The slot whose handles are showing is brighter still, so you can tell which is which.
                            const float brighten = (g_EditCollider && g_EditColliderSlot == i) ? 0.8f : 0.45f;
                            slotColor = glm::vec4(glm::mix(glm::vec3(color), glm::vec3(1.0f), brighten), 1.0f);
                        }

                        Zeyrixon::Physics::DrawColliderWireframe(collider.Slots[i], transform, slotColor);
                    }
                }

                Zeyrixon::DebugRenderer::Flush(view, projection);
            }
        }

        m_Framebuffer->Unbind();
    }

    void EditorLayer::OnEvent(Zeyrixon::Event& e)
    {
        Zeyrixon::EventDispatcher dispatcher(e);
        dispatcher.Dispatch<Zeyrixon::MouseScrolledEvent>([this](Zeyrixon::MouseScrolledEvent& event)
        {
            if (!m_ViewportHovered || m_ShowGameView)
                return false;

            m_EditorCamera.OnMouseScroll(event.GetYOffset());
            return true;
        });
    }

    void EditorLayer::DrawMenuBar()
    {
        if (!ImGui::BeginMenuBar())
            return;

        if (ImGui::BeginMenu("File"))
        {
            if (ImGui::MenuItem("New Project")) NewProject();
            if (ImGui::MenuItem("Open Project")) OpenProject();
            if (ImGui::MenuItem("Save", nullptr, false, (bool)m_ActiveProject))
            {
                if (m_ActiveProject)
                    m_ActiveProject->Save();

                if (m_ActiveWorld && !m_ActiveWorld->Save())
                    Z_EDITOR_ERROR("Failed to save world '{0}'", m_ActiveWorld->GetWorldPath());
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit"))
        {
            ImGui::MenuItem("Undo");
            ImGui::MenuItem("Redo");
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Window")) { ImGui::EndMenu(); }
        if (ImGui::BeginMenu("Help")) { ImGui::EndMenu(); }

        const float rightBlockWidth = 220.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - rightBlockWidth);

        if (ImGui::Button(m_IsPlaying ? "Stop" : "Play", ImVec2(60, 0)) && m_ActiveWorld)
        {
            if (m_IsPlaying)
            {
                m_ActiveWorld->OnRuntimeStop();
                m_IsPlaying = false;
            }
            else
            {
                // False means a script failed to compile; the errors are in the Logger panel.
                m_IsPlaying = m_ActiveWorld->OnRuntimeStart();
            }
        }

        ImGui::SameLine();
        if (ImGui::Button(m_ShowGameView ? "Game View" : "Editor View", ImVec2(140, 0)))
            m_ShowGameView = !m_ShowGameView;

        ImGui::EndMenuBar();
    }

    void EditorLayer::DrawViewportPanel()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Viewport");

        ImVec2 viewportMin = ImGui::GetCursorScreenPos();
        m_ViewportMin = viewportMin;

        ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
        if (m_ViewportSize.x != viewportPanelSize.x || m_ViewportSize.y != viewportPanelSize.y)
        {
            m_ViewportSize = viewportPanelSize;
            m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
            m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
        }

        uint32_t textureID = m_Framebuffer->GetColorAttachmentRenderID();
        ImGui::Image((void*)(intptr_t)textureID, m_ViewportSize, ImVec2(0, 1), ImVec2(1, 0));

        m_ViewportHovered = ImGui::IsWindowHovered() && !ImGuizmo::IsUsing();

        Zeyrixon::Application::Get().GetImGuiLayer()->BlockEvents(!m_ViewportHovered);

        // Light icons first, so the move/rotate gizmo of a selected light is drawn on top of its icon.
        DrawLightGizmos();
        DrawColliderGizmo();
        DrawGizmo();

        // No deleting while in Game View (the selection isn't visible there).
        if (!m_ShowGameView && DeleteKeyPressed() && m_SelectedEntity)
            DeleteEntity(m_SelectedEntity);

        // The toolbars only exist in Editor View.
        if (!m_ShowGameView)
        {
            ImVec2 padding = ImVec2(10.0f, 10.0f);
            ImGui::SetCursorScreenPos(ImVec2(viewportMin.x + padding.x, viewportMin.y + padding.y));

            ImGui::BeginGroup();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.8f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                if (ImGui::Button("\uf245"))       { m_GizmoOperation = 0; }
                ImGui::SameLine();
                if (ImGui::Button("\U000F01BE"))   { m_GizmoOperation = ImGuizmo::TRANSLATE; }
                ImGui::SameLine();
                if (ImGui::Button("\U000F0D98"))   { m_GizmoOperation = ImGuizmo::ROTATE; }
                ImGui::SameLine();
                if (ImGui::Button("\U000F0A68"))   { m_GizmoOperation = ImGuizmo::SCALE; }
                ImGui::SameLine();
                ImGui::Checkbox("Colliders", &m_ShowColliders);

                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

            ImGui::EndGroup();

            padding = ImVec2(10.0f, 10.0f);
            ImGui::SetCursorScreenPos(ImVec2(viewportPanelSize.x - padding.x, viewportMin.y + padding.y));

            float currentX = viewportMin.x + viewportPanelSize.x - padding.x;
            float targetY = viewportMin.y + padding.y;

            float itemSpacingX = 10.f;

            ImGui::BeginGroup();

                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.8f, 0.8f));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

                float btnWidth1 = 60.0f;
                currentX -= btnWidth1;
                ImGui::SetCursorScreenPos(ImVec2(currentX, targetY));
                if (ImGui::Button("2D View", ImVec2(btnWidth1, 0)))
                    m_EditorCamera.SetViewType(Editor::EditorCamera::ViewType::View2D);

                float btnWidth2 = 60.0f;
                currentX -= (btnWidth2 + itemSpacingX);
                ImGui::SetCursorScreenPos(ImVec2(currentX, targetY));
                if (ImGui::Button("3D View", ImVec2(btnWidth2, 0)))
                    m_EditorCamera.SetViewType(Editor::EditorCamera::ViewType::View3D);

                float btnWidth3 = 100.0f;
                currentX -= (btnWidth3 + itemSpacingX);
                ImGui::SetCursorScreenPos(ImVec2(currentX, targetY));

                // The text after ### is the widget ID, so it stays the same when the label changes.
                const char* viewModeName = m_EditorCamera.IsOrthographic() ? "Orthographic" : "Perspective";
                const std::string viewModeLabel = std::string(viewModeName) + "###ViewMode";

                if (ImGui::Button(viewModeLabel.c_str(), ImVec2(btnWidth3, 0)))
                    ImGui::OpenPopup("ViewModePopup");

                // Open the menu just below the button instead of at the mouse.
                ImGui::SetNextWindowPos(ImVec2(currentX, targetY + ImGui::GetFrameHeight()));
                if (ImGui::BeginPopup("ViewModePopup"))
                {
                    if (ImGui::MenuItem("Perspective", nullptr, !m_EditorCamera.IsOrthographic()))
                        m_EditorCamera.SetProjectionType(Editor::EditorCamera::ProjectionType::Perspective);

                    if (ImGui::MenuItem("Orthographic", nullptr, m_EditorCamera.IsOrthographic()))
                        m_EditorCamera.SetProjectionType(Editor::EditorCamera::ProjectionType::Orthographic);

                    ImGui::EndPopup();
                }

                ImGui::PopStyleVar();
                ImGui::PopStyleColor();

            ImGui::EndGroup();
        }
        else
        {
            // Game View overlay: the camera's name, or a hint if there isn't a primary camera.
            Zeyrixon::Entity camera = m_ActiveWorld ? m_ActiveWorld->GetPrimaryCamera() : Zeyrixon::Entity{};
            if (camera)
            {
                const std::string& name = camera.GetComponent<Zeyrixon::TagComponent>().Tag;
                ImGui::SetCursorScreenPos(ImVec2(viewportMin.x + 10.0f, viewportMin.y + 10.0f));
                ImGui::TextDisabled("Game View - %s", name.c_str());
            }
            else
            {
                const char* message = "No primary camera. Add a Camera component to an entity and tick Primary.";
                const ImVec2 textSize = ImGui::CalcTextSize(message);
                ImGui::SetCursorScreenPos(ImVec2(
                    viewportMin.x + (m_ViewportSize.x - textSize.x) * 0.5f,
                    viewportMin.y + m_ViewportSize.y * 0.5f));
                ImGui::TextDisabled("%s", message);
            }
        }

        /* Click to select. Skipped over toolbar buttons and gizmo, and while Alt is held
           (Alt + left drag orbits the camera). */
        if (m_ViewportHovered
            && !m_ShowGameView   // NEW
            && m_ActiveWorld
            && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
            && !ImGui::IsAnyItemHovered()
            && !ImGuizmo::IsOver()
            && m_HoveredColliderHandle < 0 && m_ActiveColliderHandle < 0   // clicking a resize handle is not a click on empty space
            && !Zeyrixon::Input::IsKeyPressed(Z_KEY_LEFT_ALT))
        {
            const ImVec2 mouse = ImGui::GetMousePos();
            const glm::vec2 point(mouse.x - m_ViewportMin.x, mouse.y - m_ViewportMin.y);

            glm::vec3 origin, direction;
            m_EditorCamera.ScreenPointToRay(point, glm::vec2(m_ViewportSize.x, m_ViewportSize.y), origin, direction);

            // A light icon under the mouse wins over whatever mesh is behind it. An empty click gives an
            // invalid entity, which also deselects.
            if (m_HoveredLightGizmo)
                m_SelectedEntity = m_HoveredLightGizmo;
            else
                m_SelectedEntity = Zeyrixon::Picking::PickEntity(*m_ActiveWorld, origin, direction);
        }

        ImGui::End();
        ImGui::PopStyleVar();
    }

    void EditorLayer::DrawWorldOutlinerPanel()
    {
        ImGui::Begin("World");

        if (!m_ActiveWorld)
        {
            ImGui::TextDisabled("No world loaded.");
            ImGui::End();
            return;
        }

        // Deleting while the view loop is running can invalidate it, so the
        // actual deletion happens after the loop, using this.
        Zeyrixon::Entity entityToDelete;

        if (ImGui::TreeNodeEx(m_ActiveWorld->GetName().c_str(), ImGuiTreeNodeFlags_DefaultOpen))
        {
            auto view = m_ActiveWorld->GetRegistry().view<Zeyrixon::TagComponent>();
            for (auto [handle, tag] : view.each())
            {
                Zeyrixon::Entity entity{ handle, m_ActiveWorld.get() };

                ImGui::PushID(static_cast<int>(static_cast<uint32_t>(handle)));

                if (ImGui::Selectable(tag.Tag.c_str(), m_SelectedEntity == entity))
                    m_SelectedEntity = entity;

                if (ImGui::BeginPopupContextItem())
                {
                    m_SelectedEntity = entity;
                    if (ImGui::MenuItem("Delete Entity"))
                        entityToDelete = entity;
                    ImGui::EndPopup();
                }

                ImGui::PopID();
            }
            ImGui::TreePop();
        }

        if (DeleteKeyPressed() && m_SelectedEntity)
            entityToDelete = m_SelectedEntity;

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Entity"))
                m_SelectedEntity = m_ActiveWorld->CreateEntity("Entity");

            if (ImGui::MenuItem("Create Sprite"))
            {
                Zeyrixon::Entity sprite = m_ActiveWorld->CreateEntity("Sprite");
                sprite.AddComponent<Zeyrixon::SpriteComponent>();
                m_SelectedEntity = sprite;
            }

            if (ImGui::BeginMenu("Create Light"))
            {
                // Each light type gets sensible starting values: a sun is ~3, a bulb ~10, a torch ~30
                // (point and spot lights fade with distance, so they need more to look comparable).
                if (ImGui::MenuItem("Directional Light"))
                {
                    Zeyrixon::Entity entity = m_ActiveWorld->CreateEntity("Directional Light");
                    auto& light = entity.AddComponent<Zeyrixon::LightComponent>();
                    light.Type = Zeyrixon::LightType::Directional;
                    light.Intensity = 3.0f;
                    // Pitched down 50 degrees and turned 30, so it shines down and forward.
                    entity.GetComponent<Zeyrixon::TransformComponent>().Rotation = glm::radians(glm::vec3(-50.0f, 30.0f, 0.0f));
                    m_SelectedEntity = entity;
                }

                if (ImGui::MenuItem("Point Light"))
                {
                    Zeyrixon::Entity entity = m_ActiveWorld->CreateEntity("Point Light");
                    auto& light = entity.AddComponent<Zeyrixon::LightComponent>();
                    light.Type = Zeyrixon::LightType::Point;
                    light.Intensity = 10.0f;
                    entity.GetComponent<Zeyrixon::TransformComponent>().Position = glm::vec3(0.0f, 3.0f, 0.0f);
                    m_SelectedEntity = entity;
                }

                if (ImGui::MenuItem("Spot Light"))
                {
                    Zeyrixon::Entity entity = m_ActiveWorld->CreateEntity("Spot Light");
                    auto& light = entity.AddComponent<Zeyrixon::LightComponent>();
                    light.Type = Zeyrixon::LightType::Spot;
                    light.Intensity = 30.0f;
                    auto& transform = entity.GetComponent<Zeyrixon::TransformComponent>();
                    transform.Position = glm::vec3(0.0f, 4.0f, 0.0f);
                    transform.Rotation = glm::radians(glm::vec3(-90.0f, 0.0f, 0.0f));   // straight down
                    m_SelectedEntity = entity;
                }

                ImGui::EndMenu();
            }

            ImGui::EndPopup();
        }

        if (entityToDelete)
            DeleteEntity(entityToDelete);

        ImGui::End();
    }

    void EditorLayer::DrawPropertiesPanel()
    {
        ImGui::Begin("Properties");

        if (!m_SelectedEntity)
        {
            // With nothing selected, show the settings of the world itself.
            if (m_ActiveWorld)
            {
                ImGui::SeparatorText("World");
                float ambient = m_ActiveWorld->GetAmbientIntensity();
                if (ImGui::SliderFloat("Ambient Light", &ambient, 0.0f, 1.0f))
                    m_ActiveWorld->SetAmbientIntensity(ambient);
                ImGui::TextDisabled("0 = only lights light the scene.");

                glm::vec3 gravity = m_ActiveWorld->GetGravity();
                if (ImGui::DragFloat3("Gravity", &gravity.x, 0.05f))
                    m_ActiveWorld->SetGravity(gravity);
                ImGui::TextDisabled("Units per second squared. Read when Play starts.");
            }
            else
                ImGui::TextDisabled("Nothing selected.");

            ImGui::End();
            return;
        }

        auto& components = Zeyrixon::ComponentRegistry::Get().GetAll();

        for (auto& info : components)
        {
            if (!info.DrawInspector || !info.Has(m_SelectedEntity))
                continue;

            ImGui::PushID(info.Name.c_str());

            bool open = ImGui::CollapsingHeader(info.Name.c_str(), ImGuiTreeNodeFlags_DefaultOpen);

            bool removeRequested = false;
            if (!info.Core && ImGui::BeginPopupContextItem())
            {
                if (ImGui::MenuItem("Remove Component"))
                    removeRequested = true;
                ImGui::EndPopup();
            }

            if (open)
                info.DrawInspector(m_SelectedEntity);

            if (removeRequested)
                info.Remove(m_SelectedEntity);

            ImGui::PopID();
        }

        ImGui::Separator();

        if (ImGui::Button("Add Component"))
            ImGui::OpenPopup("AddComponentPopup");

        if (ImGui::BeginPopup("AddComponentPopup"))
        {
            for (auto& info : components)
            {
                if (info.Core || info.Has(m_SelectedEntity))
                    continue;

                if (ImGui::MenuItem(info.Name.c_str()))
                    info.Add(m_SelectedEntity);
            }
            ImGui::EndPopup();
        }

        ImGui::End();
    }

    // Renders `directory`'s children as tree nodes (folders) and leaves (files).
    // Folders are only expanded, and their own children listed, when the user
    // opens them - so this stays cheap even for large trees (e.g. Engine).
    void EditorLayer::DrawDirectoryTree(const fs::path& directory)
    {
        std::error_code ec;
        if (!fs::exists(directory, ec) || !fs::is_directory(directory, ec))
        {
            ImGui::TextDisabled("(missing: %s)", directory.string().c_str());
            return;
        }

        std::vector<fs::directory_entry> dirs;
        std::vector<fs::directory_entry> files;
        ListDirectoryContents(directory, dirs, files);

        for (const auto& entry : dirs)
            DrawDirectoryNode(entry.path(), entry.path().filename().string());

        for (const auto& entry : files)
            DrawFileLeaf(entry.path());
    }

    // One folder row: arrow + icon (open/closed) + label. Clicking the row
    // (not just the arrow, thanks to SpanAvailWidth) selects it as the
    // directory the Assets panel shows. `defaultOpen` is only used for the
    // Content Browser's pseudo-roots (Assets/Your Scripts) so real subfolders
    // stay collapsed until asked for.
    void EditorLayer::DrawDirectoryNode(const fs::path& directory, const std::string& label, bool defaultOpen)
    {
        ImGui::PushID(directory.string().c_str());

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (defaultOpen)
            flags |= ImGuiTreeNodeFlags_DefaultOpen;
        if (m_SelectedDirectory == directory)
            flags |= ImGuiTreeNodeFlags_Selected;

        bool open = ImGui::TreeNodeEx("##dir", flags);
        if (ImGui::IsItemClicked())
            m_SelectedDirectory = directory;

        ImGui::SameLine();
        const auto& folderIcon = open ? m_FolderOpenIcon : m_FolderClosedIcon;
        if (folderIcon && folderIcon->IsLoaded())
            ImGui::Image((void*)(intptr_t)folderIcon->GetRendererID(), ImVec2(16.0f, 16.0f), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::TextUnformatted(label.c_str());

        if (open)
        {
            DrawDirectoryTree(directory);
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // One file row: icon + name, no expand arrow. Mirrors DrawDirectoryNode's
    // call order (hidden TreeNodeEx first, then icon/text via SameLine) so the
    // icon reserves the same arrow-slot spacing a folder row gets - otherwise
    // file icons sit noticeably further left than folder icons at the same depth.
    void EditorLayer::DrawFileLeaf(const fs::path& file)
    {
        ImGui::PushID(file.string().c_str());

        ImGui::TreeNodeEx("##file", ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth);

        ImGui::SameLine();
        if (m_FileIcon && m_FileIcon->IsLoaded())
            ImGui::Image((void*)(intptr_t)m_FileIcon->GetRendererID(), ImVec2(16.0f, 16.0f), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::SameLine();
        ImGui::TextUnformatted(file.filename().string().c_str());

        ImGui::PopID();
    }

    void EditorLayer::DrawLoggerPanel()
    {
        ImGui::Begin("Logger");

        auto buffer = Zeyrixon::Log::GetBuffer();
        if (!buffer)
        {
            ImGui::TextDisabled("Logging isn't initialised.");
            ImGui::End();
            return;
        }

        // Only copy the entries again when something new was logged.
        const uint64_t version = buffer->GetVersion();
        if (version != m_LogVersion)
        {
            buffer->CopyEntries(m_LogEntries);
            m_LogVersion = version;
        }

        if (ImGui::Button("Clear"))
            buffer->Clear();

        ImGui::SameLine();
        ImGui::Checkbox("Auto-scroll", &m_LogAutoScroll);

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        ImGui::Combo("##LogLevel", &m_LogMinLevelIndex, k_LogLevelNames, IM_ARRAYSIZE(k_LogLevelNames));

        ImGui::SameLine();
        m_LogFilter.Draw("Filter", 200.0f);

        ImGui::Separator();

        const spdlog::level::level_enum minLevel = k_LogLevelValues[m_LogMinLevelIndex];

        // Which entries pass the level and text filters.
        static std::vector<int> visible;
        visible.clear();
        for (int i = 0; i < static_cast<int>(m_LogEntries.size()); ++i)
        {
            const Zeyrixon::LogEntry& entry = m_LogEntries[i];

            if (entry.Level < minLevel)
                continue;

            if (m_LogFilter.IsActive()
                && !m_LogFilter.PassFilter(entry.Message.c_str())
                && !m_LogFilter.PassFilter(entry.Logger.c_str()))
                continue;

            visible.push_back(i);
        }

        // Scrolls horizontally instead of wrapping, which keeps every row the same height
        // so the clipper below can skip the rows that aren't on screen.
        ImGui::BeginChild("LogScroll", ImVec2(0.0f, 0.0f), 0, ImGuiWindowFlags_HorizontalScrollbar);

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step())
        {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row)
            {
                const Zeyrixon::LogEntry& entry = m_LogEntries[visible[row]];

                ImGui::PushStyleColor(ImGuiCol_Text, LogLevelColor(entry.Level));
                ImGui::Text("[%s] %s: %s", entry.Time.c_str(), entry.Logger.c_str(), entry.Message.c_str());
                ImGui::PopStyleColor();
            }
        }
        clipper.End();

        // Stay pinned to the newest message, but only if the view was already at the bottom,
        // so scrolling up to read something isn't yanked away.
        if (m_LogAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();

        ImGui::End();
    }

    void EditorLayer::DrawContentBrowserPanel()
    {
        ImGui::Begin("Content Browser");

        if (m_ActiveProject)
        {
            if (ImGui::TreeNodeEx("Content", ImGuiTreeNodeFlags_DefaultOpen))
            {
                DrawDirectoryNode(m_ActiveProject->GetAssetDirectory(), "Assets", true);
                DrawDirectoryNode(m_ActiveProject->GetCodeDirectory(), "Your Scripts", true);
                ImGui::TreePop();
            }
        }

        // Engine's own source tree, rooted at the "Zeyrixon" engine module
        // (not the whole solution, which would also pull in ZeyrixonEditor/vendor/.git).
        fs::path engineRoot = fs::path(Z_ROOT_PATH) / "Zeyrixon";
        std::error_code engineEc;
        if (fs::is_directory(engineRoot, engineEc) && ImGui::TreeNodeEx("Engine", ImGuiTreeNodeFlags_DefaultOpen))
        {
            DrawDirectoryTree(engineRoot);
            ImGui::TreePop();
        }

        ImGui::End();
    }

    void EditorLayer::DrawAssetsPanel()
    {
        ImGui::Begin("Assets");

        if (!m_ActiveProject)
        {
            ImGui::TextDisabled("No project loaded.");
            ImGui::End();
            return;
        }

        // Nothing picked in the Content Browser yet (or a project was just
        // loaded) - default to the project's Assets folder.
        if (m_SelectedDirectory.empty())
            m_SelectedDirectory = m_ActiveProject->GetAssetDirectory();

        ImGui::TextDisabled("%s", m_SelectedDirectory.string().c_str());
        ImGui::Separator();

        std::error_code ec;
        if (!fs::exists(m_SelectedDirectory, ec) || !fs::is_directory(m_SelectedDirectory, ec))
        {
            ImGui::TextDisabled("(folder no longer exists)");
            ImGui::End();
            return;
        }

        std::vector<fs::directory_entry> dirs;
        std::vector<fs::directory_entry> files;
        ListDirectoryContents(m_SelectedDirectory, dirs, files);

        const float thumbnailSize = 64.0f;
        const float cellPadding = 16.0f;
        const float cellSize = thumbnailSize + cellPadding;

        float panelWidth = ImGui::GetContentRegionAvail().x;
        int columnCount = (int)(panelWidth / cellSize);
        if (columnCount < 1)
            columnCount = 1;

        ImGui::Columns(columnCount, nullptr, false);

        // Set by a cell when it is right-clicked; the single shared popup is opened after the grid is drawn.
        bool openItemPopup = false;

        // One grid cell: icon button on top, name wrapped/centered underneath.
        auto drawCell = [&](const fs::path& path, bool isDir)
        {
            ImGui::PushID(path.string().c_str());

            const auto& icon = isDir ? m_FolderClosedIcon : m_FileIcon;
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.08f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.15f));

            if (icon && icon->IsLoaded())
                ImGui::ImageButton("##thumb", (void*)(intptr_t)icon->GetRendererID(), ImVec2(thumbnailSize, thumbnailSize), ImVec2(0, 1), ImVec2(1, 0));
            else
                ImGui::Button("##thumb", ImVec2(thumbnailSize, thumbnailSize));

            ImGui::PopStyleColor(3);

            // Double-click a folder to navigate into it.
            if (isDir && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
                m_SelectedDirectory = path;

            // Right-click released over THIS cell: remember it and ask for the menu.
            if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            {
                m_ContextAsset = path;
                openItemPopup = true;
            }

            std::string name = path.filename().string();
            float startX = ImGui::GetCursorPosX();
            ImVec2 textSize = ImGui::CalcTextSize(name.c_str());
            if (textSize.x < thumbnailSize)
                ImGui::SetCursorPosX(startX + (thumbnailSize - textSize.x) * 0.5f);

            ImGui::PushTextWrapPos(startX + thumbnailSize);
            ImGui::TextWrapped("%s", name.c_str());
            ImGui::PopTextWrapPos();

            ImGui::NextColumn();
            ImGui::PopID();
        };

        for (const auto& entry : dirs)
            drawCell(entry.path(), true);

        for (const auto& entry : files)
            drawCell(entry.path(), false);

        ImGui::Columns(1);

        // One popup for the whole panel, opened outside any PushID so OpenPopup/BeginPopup agree on the ID.
        if (openItemPopup)
            ImGui::OpenPopup("AssetItemPopup");

        if (ImGui::BeginPopup("AssetItemPopup"))
        {
            ImGui::TextDisabled("%s", m_ContextAsset.filename().string().c_str());
            ImGui::Separator();

            if (ImGui::MenuItem("Rename"))
            {
                Z_EDITOR_TRACE("Rename clicked for '{0}'", m_ContextAsset.string());
                const std::string currentName = m_ContextAsset.filename().string();
                strncpy(m_RenameBuffer, currentName.c_str(), sizeof(m_RenameBuffer) - 1);
                m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
                m_RenameError.clear();
                m_OpenRenamePopup = true;   // opened below, outside this popup
            }

            if (ImGui::MenuItem("Delete"))
            {
                std::error_code rmEc;
                if (fs::is_directory(m_ContextAsset, rmEc))
                    fs::remove_all(m_ContextAsset, rmEc);
                else
                    fs::remove(m_ContextAsset, rmEc);

                if (rmEc)
                    Z_EDITOR_WARN("Failed to remove '{0}': {1}", m_ContextAsset.string(), rmEc.message());
                else
                    Z_EDITOR_TRACE("Removed '{0}'", m_ContextAsset.string());

                m_ContextAsset.clear();
            }
            ImGui::EndPopup();
        }

        // Rename dialog. OpenPopup runs here, at panel level, so its ID matches BeginPopupModal below.
        if (m_OpenRenamePopup)
        {
            ImGui::OpenPopup("Rename Asset");
            m_OpenRenamePopup = false;
        }

        if (ImGui::BeginPopupModal("Rename Asset", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            bool confirm = ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer),
                ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);

            if (!m_RenameError.empty())
                ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_RenameError.c_str());

            if (ImGui::Button("OK"))
                confirm = true;
            ImGui::SameLine();
            const bool cancel = ImGui::Button("Cancel");

            if (confirm)
            {
                const fs::path newName = m_RenameBuffer;
                const fs::path target = m_ContextAsset.parent_path() / newName;
                std::error_code renameEc;

                if (newName.empty() || newName.filename() != newName)
                    m_RenameError = "Enter a plain file or folder name.";
                else if (target == m_ContextAsset)
                    ImGui::CloseCurrentPopup();   // name unchanged
                else if (fs::exists(target, renameEc))
                    m_RenameError = "Something with that name already exists.";
                else
                {
                    fs::rename(m_ContextAsset, target, renameEc);
                    if (renameEc)
                        m_RenameError = renameEc.message();
                    else
                    {
                        Z_EDITOR_TRACE("Renamed '{0}' to '{1}'", m_ContextAsset.string(), target.string());
                        m_ContextAsset.clear();
                        ImGui::CloseCurrentPopup();
                    }
                }
            }

            if (cancel)
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
        {
            if (ImGui::MenuItem("Create Folder"))
            {
                if (fs::create_directory(m_SelectedDirectory / "New Folder"))
                    Z_EDITOR_TRACE("Created a folder at: '{0}'", m_SelectedDirectory.string());
                else
                    Z_EDITOR_WARN("Failed to create a folder at: '{0}'", m_SelectedDirectory.string());
            }
            if (ImGui::MenuItem("Create World"))
            {
                std::filesystem::path filePath =
                    std::filesystem::path(m_SelectedDirectory) / "NewWorld.zworld";

                std::ofstream newWorld(filePath);

                if (!newWorld)
                    Z_EDITOR_WARN("Failed to create World!");

                newWorld << R"(<?xml version="1.0"?>)" << "\n";
                newWorld << R"(<World Name="NewWorld" Ambient="0" Gravity="0 -9.81000042 0">)" << "\n";
                newWorld << "</World>\n";
            }
            if (ImGui::MenuItem("Create Script"))
            {
                std::filesystem::path filePath =
                    std::filesystem::path(m_SelectedDirectory) / "NewScript.as";

                std::ofstream newScript(filePath);

                if (!newScript)
                    Z_EDITOR_WARN("Failed to create Script!");

                newScript << "class NewScript\n";
                newScript << R"({)" << "\n";
                newScript << R"(    void OnCreate())" << "\n";
                newScript << R"(    {)" << "\n";
                newScript << R"(        Log::Trace("Hello World!");)" << "\n";
                newScript << R"(    })" << "\n";
                newScript << R"(    void OnUpdate(float dt))" << "\n";
                newScript << R"(    {)" << "\n";
                newScript << R"(        Log::Trace("Script Updated");)" << "\n";
                newScript << R"(    })" << "\n";
                newScript << R"(    void OnDestroy())" << "\n";
                newScript << R"(    {)" << "\n";
                newScript << R"(        Log::Trace("Script destroyed!");)" << "\n";
                newScript << R"(    })" << "\n";
                newScript << R"(})" << "\n";
            }

            ImGui::EndPopup();
        }

        ImGui::End();
    }

    void EditorLayer::LoadStartupWorld()
    {
        // Replacing the world while it is playing would leave scripts pointing at a dead world.
        if (m_IsPlaying && m_ActiveWorld)
            m_ActiveWorld->OnRuntimeStop();
        m_IsPlaying = false;

        Zeyrixon::ScriptEngine::SetAssetRoot(m_ActiveProject->GetAssetDirectory());

        m_SelectedEntity = {};
        m_ActiveWorld = Zeyrixon::World::Load(m_ActiveProject->GetStartupWorldPath());

        if (!m_ActiveWorld)
            Z_EDITOR_ERROR("Failed to load startup world for project '{0}'", m_ActiveProject->GetName());
    }

    void EditorLayer::LoadWorld(std::string worldPath)
    {
        if (m_IsPlaying && m_ActiveWorld)
            m_ActiveWorld->OnRuntimeStop();
        m_IsPlaying = false;

        Zeyrixon::ScriptEngine::SetAssetRoot(m_ActiveProject->GetAssetDirectory());

        m_SelectedEntity = {};
        m_ActiveWorld = Zeyrixon::World::Load(worldPath);

        if (!m_ActiveWorld)
            Z_EDITOR_ERROR("Failed to load world for project '{0}'", m_ActiveProject->GetName());
    }
}