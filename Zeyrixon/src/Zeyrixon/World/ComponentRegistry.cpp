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
            node.append_attribute("FOV") = c.FOV;
            node.append_attribute("Near") = c.Near;
            node.append_attribute("Far") = c.Far;
            node.append_attribute("Primary") = c.Primary;
        };
        camera.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& c = e.GetComponent<CameraComponent>();
            c.FOV = node.attribute("FOV").as_float(c.FOV);
            c.Near = node.attribute("Near").as_float(c.Near);
            c.Far = node.attribute("Far").as_float(c.Far);
            c.Primary = node.attribute("Primary").as_bool(c.Primary);
        };

        auto& mesh = Register<MeshComponent>("Mesh");
        mesh.Serialize = [](Entity e, pugi::xml_node node)
        {
            auto& m = e.GetComponent<MeshComponent>();
            node.append_attribute("Path") = m.MeshPath.c_str();
            node.append_attribute("Color") = Vec3ToString(m.Color).c_str();
        };
        mesh.Deserialize = [](Entity e, pugi::xml_node node)
        {
            auto& m = e.GetComponent<MeshComponent>();
            const std::string fallbackPath = m.MeshPath;
            m.MeshPath = node.attribute("Path").as_string(fallbackPath.c_str());
            m.Color = StringToVec3(node.attribute("Color").as_string(""), m.Color);
        };
    }
}
