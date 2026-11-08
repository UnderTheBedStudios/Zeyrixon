#pragma once

#include <ZeyrixonEditor/Commands/Command.h>
#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/World/ComponentRegistry.h>

#include <pugixml.hpp>
#include <sstream>
#include <string>

namespace Editor
{
    // ---- A component, or a whole entity, as XML text ----

    inline std::string DocToString(const pugi::xml_document& doc)
    {
        std::ostringstream out;
        doc.save(out, "", pugi::format_raw);
        return out.str();
    }

    inline std::string SnapshotComponent(const Zeyrixon::ComponentInfo& info, Zeyrixon::Entity entity)
    {
        if (!info.Serialize || !info.Has(entity))
            return {};
        pugi::xml_document doc;
        info.Serialize(entity, doc.append_child("C"));
        return DocToString(doc);
    }

    inline void RestoreComponent(const Zeyrixon::ComponentInfo& info, Zeyrixon::Entity entity, const std::string& xml)
    {
        pugi::xml_document doc;
        if (info.Deserialize && doc.load_string(xml.c_str()))
            info.Deserialize(entity, doc.child("C"));
    }

    inline std::string SnapshotEntity(Zeyrixon::Entity entity)
    {
        pugi::xml_document doc;
        pugi::xml_node root = doc.append_child("Entity");
        for (auto& info : Zeyrixon::ComponentRegistry::Get().GetAll())
            if (info.Serialize && info.Has(entity))
                info.Serialize(entity, root.append_child(info.Name.c_str()));
        return DocToString(doc);
    }

    // Same loop as World::Load, so a restored entity comes back exactly like a loaded one.
    inline void RestoreEntity(Zeyrixon::World& world, uint64_t id, const std::string& xml)
    {
        pugi::xml_document doc;
        if (!doc.load_string(xml.c_str()))
            return;
        pugi::xml_node root = doc.child("Entity");

        Zeyrixon::Entity entity = world.CreateEntityWithID(id);
        for (auto& info : Zeyrixon::ComponentRegistry::Get().GetAll())
        {
            pugi::xml_node node = root.child(info.Name.c_str());
            if (!node && !info.Core)
                continue;
            if (!info.Has(entity))
                info.Add(entity);
            if (info.Deserialize)
                info.Deserialize(entity, node);
        }
    }

    // ---- Commands ----

    // One component of one entity went from `before` to `after`.
    class EditComponentCommand : public Command
    {
    public:
        EditComponentCommand(Zeyrixon::World& world, uint64_t id, std::string component,
                             std::string before, std::string after)
            : m_World(world), m_ID(id), m_Component(std::move(component)),
              m_Before(std::move(before)), m_After(std::move(after)) {}

        void Execute() override { Apply(m_After); }
        void Undo() override    { Apply(m_Before); }
        const char* GetName() const override { return "Edit Component"; }

    private:
        void Apply(const std::string& xml)
        {
            Zeyrixon::Entity entity = m_World.FindEntityByID(m_ID);
            auto* info = Zeyrixon::ComponentRegistry::Get().Find(m_Component);
            if (entity && info && info->Has(entity))
                RestoreComponent(*info, entity, xml);
        }

        Zeyrixon::World& m_World;
        uint64_t m_ID;
        std::string m_Component, m_Before, m_After;
    };

    /* Creating or deleting an entity. The snapshot is taken in the constructor.
       Create: build the entity fully, then history.Record(...)  (it already exists)
       Delete: history.Do(...) (snapshots, then Execute destroys it) */
    class EntityExistsCommand : public Command
    {
    public:
        EntityExistsCommand(Zeyrixon::World& world, Zeyrixon::Entity entity, bool createsOnExecute)
            : m_World(world), m_ID(entity.GetComponent<Zeyrixon::IDComponent>().ID),
              m_Xml(SnapshotEntity(entity)), m_Creates(createsOnExecute) {}

        void Execute() override { if (m_Creates) Create(); else Destroy(); }
        void Undo() override    { if (m_Creates) Destroy(); else Create(); }
        const char* GetName() const override { return m_Creates ? "Create Entity" : "Delete Entity"; }

    private:
        void Create()
        {
            if (!m_World.FindEntityByID(m_ID))
                RestoreEntity(m_World, m_ID, m_Xml);
        }
        void Destroy()
        {
            Zeyrixon::Entity entity = m_World.FindEntityByID(m_ID);
            if (entity)
                m_World.DestroyEntity(entity);
        }

        Zeyrixon::World& m_World;
        uint64_t m_ID;
        std::string m_Xml;
        bool m_Creates;
    };
}