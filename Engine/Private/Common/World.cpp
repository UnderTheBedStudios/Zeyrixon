#include <Engine/Public/Common/World.h>
#include <pugixml.hpp>
#include <cstdio>

#include "Engine/Public/Engine.h"

static const char* kNumericsNs = "http://schemas.datacontract.org/2004/07/System.Numerics";

static glm::vec3 ParseVec3(const pugi::xml_node& node)
{
    glm::vec3 v(0.0f);
    if (!node) return v;
    v.x = node.child("d2p1:X").text().as_float();
    v.y = node.child("d2p1:Y").text().as_float();
    v.z = node.child("d2p1:Z").text().as_float();
    return v;
}

static void WriteVec3(pugi::xml_node& parent, const char* tagName, const glm::vec3& v)
{
    pugi::xml_node node = parent.append_child(tagName);
    node.append_attribute("xmlns:d2p1") = kNumericsNs;
    node.append_child("d2p1:X").text().set(v.x);
    node.append_child("d2p1:Y").text().set(v.y);
    node.append_child("d2p1:Z").text().set(v.z);
}

bool World::LoadFromFile(const std::filesystem::path& zworldPath)
{
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(zworldPath.c_str());
    if (!result)
    {
    	fprintf(stderr, "[World] tried to get location from \"%s\" but failed\n",
    			zworldPath.string().c_str());
        return false;
    }

    pugi::xml_node worldNode = doc.child("World");
    if (!worldNode)
    {
        fprintf(stderr, "[World] %s has no <World> root node\n", zworldPath.string().c_str());
        return false;
    }

    std::string name = worldNode.child("WorldName").text().as_string();
    glm::vec3 lightDir = ParseVec3(worldNode.child("LightDir"));
    glm::vec3 lightColor = ParseVec3(worldNode.child("LightColor"));
    std::string defaultCameraName = worldNode.child("DefaultCamera").text().as_string();

    std::vector<WorldEntity> entities;
    pugi::xml_node entitiesNode = worldNode.child("Entities");
    for (pugi::xml_node entityNode : entitiesNode.children("Entity"))
    {
        WorldEntity e;
        e.type = entityNode.child("Type").text().as_string();
        e.name = entityNode.child("Name").text().as_string();
        e.position = ParseVec3(entityNode.child("Position"));
        e.rotation = ParseVec3(entityNode.child("Rotation"));
        e.scale = ParseVec3(entityNode.child("Scale"));
        e.hasModel = entityNode.child("HasModel").text().as_bool();
        e.modelPath = entityNode.child("ModelPath").text().as_string();
    	e.index = entityNode.child("Index").text().as_int();
    	if (e.hasModel)
    	{
    		fprintf(stdout, "True: %s\n", e.name.c_str());
    		if (Engine_SetEntityModelPath(e.index, e.modelPath.c_str()))
    			fprintf(stdout, "Works.\n");
    		else
    			fprintf(stderr, "NEIN!: %s\n", Engine_SetEntityModelPath(e.index, e.modelPath.c_str()));
    	}
        entities.push_back(std::move(e));
    }

    // Only commit to member state once parsing has fully succeeded, so a bad file
    // can't leave this object half-updated.
    m_zworldPath = zworldPath;
    m_name = std::move(name);
    m_lightDir = lightDir;
    m_lightColor = lightColor;
    m_defaultCameraName = std::move(defaultCameraName);
    m_entities = std::move(entities);
    return true;
}

bool World::SaveToFile(std::filesystem::path zworldPath)
{
    if (zworldPath.empty())
        zworldPath = m_zworldPath;

    if (zworldPath.empty())
    {
        fprintf(stderr, "[World] SaveToFile called with no path (and m_zworldPath is unset)\n");
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_node worldNode = doc.append_child("World");
    worldNode.append_attribute("z:Id") = "i2";

    worldNode.append_child("WorldName").text().set(m_name.c_str());
    WriteVec3(worldNode, "LightDir", m_lightDir);
    WriteVec3(worldNode, "LightColor", m_lightColor);
    worldNode.append_child("DefaultCamera").text().set(m_defaultCameraName.c_str());

    pugi::xml_node entitiesNode = worldNode.append_child("Entities");
    for (const WorldEntity& e : m_entities)
    {
        pugi::xml_node entityNode = entitiesNode.append_child("Entity");
        entityNode.append_child("Type").text().set(e.type.c_str());
        entityNode.append_child("Name").text().set(e.name.c_str());
        WriteVec3(entityNode, "Position", e.position);
        WriteVec3(entityNode, "Rotation", e.rotation);
        WriteVec3(entityNode, "Scale", e.scale);
        entityNode.append_child("HasModel").text().set(e.hasModel);
        entityNode.append_child("ModelPath").text().set(e.modelPath.c_str());
    }

    pugi::xml_node projectRef = worldNode.append_child("_project");
    projectRef.append_attribute("z:Ref") = "i1";

    if (!doc.save_file(zworldPath.c_str(), "\t"))
    {
        fprintf(stderr, "[World] Failed to write %s\n", zworldPath.string().c_str());
        return false;
    }

    m_zworldPath = zworldPath;
    fprintf(stdout, "[World] Saved \"%s\" to %s\n", m_name.c_str(), zworldPath.string().c_str());
    return true;
}
