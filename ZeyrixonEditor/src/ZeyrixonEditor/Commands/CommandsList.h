#pragma once

#include <ZeyrixonEditor/Commands/Command.h>
#include <Zeyrixon/World/World.h>
#include <Zeyrixon/World/Entity.h>
#include <Zeyrixon/World/Components.h>
#include <Zeyrixon/World/ComponentRegistry.h>

#include <pugixml.hpp>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

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
        const uint64_t parentID = entity.GetComponent<Zeyrixon::IDComponent>().ParentID;
        if (parentID != 0)
            root.append_attribute("Parent") = static_cast<unsigned long long>(parentID);
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

        // Back under its parent, if that still exists. Children are restored after their parents (see
        // EntityExistsCommand), so this finds it. The local transform was just restored, so keep it as it is.
        const uint64_t parentID = root.attribute("Parent").as_ullong(0);
        if (parentID != 0)
            world.SetParent(entity, world.FindEntityByID(parentID), false);
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

    /* Creating or deleting an entity, together with everything under it (deleting a parent deletes its
       children, so undoing that has to bring them all back). The snapshots are taken in the constructor.
       Create: build the entity fully, then history.Record(...)  (it already exists)
       Delete: history.Do(...) (snapshots, then Execute destroys it) */
    class EntityExistsCommand : public Command
    {
    public:
        EntityExistsCommand(Zeyrixon::World& world, Zeyrixon::Entity entity, bool createsOnExecute)
            : m_World(world), m_ID(entity.GetComponent<Zeyrixon::IDComponent>().ID), m_Creates(createsOnExecute)
        {
            // Parents come before their children, so restoring in this order always finds the parent.
            for (uint64_t id : world.GetSubtreeIDs(m_ID))
                m_Snapshots.emplace_back(id, SnapshotEntity(world.FindEntityByID(id)));
        }

        void Execute() override { if (m_Creates) Create(); else Destroy(); }
        void Undo() override    { if (m_Creates) Destroy(); else Create(); }
        const char* GetName() const override { return m_Creates ? "Create Entity" : "Delete Entity"; }

    private:
        void Create()
        {
            for (const auto& [id, xml] : m_Snapshots)
                if (!m_World.FindEntityByID(id))
                    RestoreEntity(m_World, id, xml);
        }
        void Destroy()
        {
            Zeyrixon::Entity entity = m_World.FindEntityByID(m_ID);
            if (entity)
                m_World.DestroyEntity(entity);   // takes the children with it
        }

        Zeyrixon::World& m_World;
        uint64_t m_ID;
        std::vector<std::pair<uint64_t, std::string>> m_Snapshots;   // root first, then its descendants
        bool m_Creates;
    };

    /* Dragging an entity under another one (or out to the top level). The first Execute keeps the entity where
       it is on screen (World::SetParent with keepWorldTransform) and remembers the local transform that
       produced; Undo and redo then put back exactly the stored parents and local transforms. */
    class SetParentCommand : public Command
    {
    public:
        SetParentCommand(Zeyrixon::World& world, Zeyrixon::Entity child, Zeyrixon::Entity newParent)
            : m_World(world), m_ChildID(child.GetComponent<Zeyrixon::IDComponent>().ID),
              m_OldParentID(child.GetComponent<Zeyrixon::IDComponent>().ParentID),
              m_NewParentID(newParent ? newParent.GetComponent<Zeyrixon::IDComponent>().ID : 0)
        {
            if (auto* info = Zeyrixon::ComponentRegistry::Get().Find("Transform"))
                m_Before = SnapshotComponent(*info, child);
        }

        void Execute() override
        {
            Zeyrixon::Entity child = m_World.FindEntityByID(m_ChildID);
            if (!child)
                return;

            auto* info = Zeyrixon::ComponentRegistry::Get().Find("Transform");
            if (!m_HaveAfter)
            {
                m_World.SetParent(child, m_World.FindEntityByID(m_NewParentID), true);
                if (info)
                    m_After = SnapshotComponent(*info, child);
                m_HaveAfter = true;
            }
            else
            {
                m_World.SetParent(child, m_World.FindEntityByID(m_NewParentID), false);
                if (info && !m_After.empty())
                    RestoreComponent(*info, child, m_After);
            }
        }

        void Undo() override
        {
            Zeyrixon::Entity child = m_World.FindEntityByID(m_ChildID);
            if (!child)
                return;

            m_World.SetParent(child, m_World.FindEntityByID(m_OldParentID), false);
            if (auto* info = Zeyrixon::ComponentRegistry::Get().Find("Transform"))
                if (!m_Before.empty())
                    RestoreComponent(*info, child, m_Before);
        }

        const char* GetName() const override { return "Set Parent"; }

    private:
        Zeyrixon::World& m_World;
        uint64_t m_ChildID, m_OldParentID, m_NewParentID;
        std::string m_Before, m_After;   // the child's local Transform, as XML
        bool m_HaveAfter = false;
    };
}