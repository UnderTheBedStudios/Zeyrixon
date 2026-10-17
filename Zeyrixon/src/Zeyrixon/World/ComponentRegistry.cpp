#include <pch.h>

#include <Zeyrixon/World/ComponentRegistry.h>
#include <Zeyrixon/World/Components.h>

#include <cstdio>

namespace{
    std::string Vec3ToString(const glm::vec3& v)
    {
        char buffer[96];
        std::snprintf(buffer, sizeof(buffer), "%.9g %.9g %.9g", v.x, v.y, v.z);
        return buffer;
    }

    glm::vec3 StringToVec3(const char* text, const glm::vec3& fallback)
    {
        glm::vec3 v;
        if (std::sscanf(text, "%f %f %f", &v.x, &v.y, &v.z) != 3)
            return fallback;
        return v;
    }

    std::string Vec4ToString(const glm::vec4& v)
    {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "%.9g %.9g %.9g %.9g", v.x, v.y, v.z, v.w);
        return buffer;
    }

    glm::vec4 StringToVec4(const char* text, const glm::vec4& fallback)
    {
        glm::vec4 v;
        if (std::sscanf(text, "%f %f %f %f", &v.x, &v.y, &v.z, &v.w) != 4)
            return fallback;
        return v;
    }
}

namespace Zeyrixon
{
    ComponentRegistry& ComponentRegistry::Get()
    {
        static ComponentRegistry instance;
        return instance;
    }

    ComponentInfo* ComponentRegistry::Find(const std::string& name)
    {
        for (auto& info : m_Components)
            if (info.Name == name)
                return &info;
        return nullptr;
    }

    ComponentRegistry::ComponentRegistry()
    {
        // The ID is saved as an attribute of the <Entity> node by World itself,
        // so it has no Serialize/Deserialize here.
        Register<IDComponent>("ID", true);

        auto& tag = Register<TagComponent>("Tag", true);
        tag.Serialize = [](Entity e, pugi::xml_node node)
        {
            node.append_attribute("Value") = e.GetComponent<TagComponent>().Tag.c_str();
        };
        tag.Deserialize = [](Entity e, pugi::xml_node node)
        {
            e.GetComponent<TagComponent>().Tag = node.attribute("Value").as_string("Entity");
        };

        auto& transform = Register<TransformComponent>("Transform", true);
        transform.Serialize = [](Entity e, pugi::xml_node node)
        {
            auto& t = e.GetComponent<TransformComponent>();
            node.append_attribute("Position") = Vec3ToString(t.Position).c_str();
            node.append_attribute("Rotation") = Vec3ToString(t.Rotation).c_str();
            node.append_attribute("Scale")    = Vec3ToString(t.Scale).c_str();
        };
        transform.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& t = e.GetComponent<TransformComponent>();
            t.Position = StringToVec3(node.attribute("Position").as_string(""), t.Position);
            t.Rotation = StringToVec3(node.attribute("Rotation").as_string(""), t.Rotation);
            t.Scale    = StringToVec3(node.attribute("Scale").as_string(""),    t.Scale);
        };

        auto& camera = Register<CameraComponent>("Camera");
        camera.Serialize = [](Entity e, pugi::xml_node node)
        {
            auto& c = e.GetComponent<CameraComponent>();
            node.append_attribute("Orthographic") = c.Orthographic;
            node.append_attribute("OrthographicSize") = c.OrthographicSize;
            node.append_attribute("FOV") = c.FOV;
            node.append_attribute("Near") = c.Near;
            node.append_attribute("Far") = c.Far;
            node.append_attribute("Primary") = c.Primary;
        };
        camera.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& c = e.GetComponent<CameraComponent>();
            c.Orthographic = node.attribute("Orthographic").as_bool(c.Orthographic);
            c.OrthographicSize = node.attribute("OrthographicSize").as_float(c.OrthographicSize);
            c.FOV = node.attribute("FOV").as_float(c.FOV);
            c.Near = node.attribute("Near").as_float(c.Near);
            c.Far = node.attribute("Far").as_float(c.Far);
            c.Primary = node.attribute("Primary").as_bool(c.Primary);
        };

        auto& mesh = Register<MeshComponent>("Mesh");
        mesh.Serialize = [](Entity e, pugi::xml_node node)
        {
            node.append_attribute("Path") = e.GetComponent<MeshComponent>().MeshPath.c_str();
        };
        mesh.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& m = e.GetComponent<MeshComponent>();
            const std::string fallbackPath = m.MeshPath;
            m.MeshPath = node.attribute("Path").as_string(fallbackPath.c_str());
        };

        auto& material = Register<MaterialComponent>("Material");
        material.Serialize = [](Entity e, pugi::xml_node node)
        {
            auto& m = e.GetComponent<MaterialComponent>();
            node.append_attribute("Albedo") = Vec3ToString(m.Albedo).c_str();
            node.append_attribute("Metallic") = m.Metallic;
            node.append_attribute("Roughness") = m.Roughness;
            node.append_attribute("AO") = m.AO;
            node.append_attribute("AlbedoMap") = m.AlbedoMap.c_str();
            node.append_attribute("NormalMap") = m.NormalMap.c_str();
            node.append_attribute("RoughnessMap") = m.RoughnessMap.c_str();
            node.append_attribute("MetallicMap") = m.MetallicMap.c_str();
            node.append_attribute("AOMap") = m.AOMap.c_str();
        };
        material.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& m = e.GetComponent<MaterialComponent>();
            m.Albedo = StringToVec3(node.attribute("Albedo").as_string(""), m.Albedo);
            m.Metallic = node.attribute("Metallic").as_float(m.Metallic);
            m.Roughness = node.attribute("Roughness").as_float(m.Roughness);
            m.AO = node.attribute("AO").as_float(m.AO);
            m.AlbedoMap = node.attribute("AlbedoMap").as_string();
            m.NormalMap = node.attribute("NormalMap").as_string();
            m.RoughnessMap = node.attribute("RoughnessMap").as_string();
            m.MetallicMap = node.attribute("MetallicMap").as_string();
            m.AOMap = node.attribute("AOMap").as_string();
        };

        auto& script = Register<ScriptComponent>("Script");
        script.Serialize = [](Entity e, pugi::xml_node node)
        {
            auto& s = e.GetComponent<ScriptComponent>();
            node.append_attribute("Path") = s.ScriptPath.c_str();
            node.append_attribute("Class") = s.ClassName.c_str();
        };
        script.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& s = e.GetComponent<ScriptComponent>();
            s.ScriptPath = node.attribute("Path").as_string();
            s.ClassName = node.attribute("Class").as_string();
        };

        auto& sprite = Register<SpriteComponent>("Sprite");
        sprite.Serialize = [](Entity e, pugi::xml_node node)
        {
            auto& s = e.GetComponent<SpriteComponent>();
            node.append_attribute("Texture") = s.TexturePath.c_str();
            node.append_attribute("Color") = Vec4ToString(s.Color).c_str();
            node.append_attribute("PixelsPerUnit") = s.PixelsPerUnit;
            node.append_attribute("FlipX") = s.FlipX;
            node.append_attribute("FlipY") = s.FlipY;
            node.append_attribute("PixelArt") = s.PixelArt;
            node.append_attribute("SortingOrder") = s.SortingOrder;
        };
        sprite.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& s = e.GetComponent<SpriteComponent>();
            s.TexturePath = node.attribute("Texture").as_string();
            s.Color = StringToVec4(node.attribute("Color").as_string(""), s.Color);
            s.PixelsPerUnit = node.attribute("PixelsPerUnit").as_float(s.PixelsPerUnit);
            s.FlipX = node.attribute("FlipX").as_bool(s.FlipX);
            s.FlipY = node.attribute("FlipY").as_bool(s.FlipY);
            s.PixelArt = node.attribute("PixelArt").as_bool(s.PixelArt);
            s.SortingOrder = node.attribute("SortingOrder").as_int(s.SortingOrder);
        };
    }
}
