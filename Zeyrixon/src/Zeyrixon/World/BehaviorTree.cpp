#include <Zeyrixon/World/BehaviorTree.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/World/Entity.h>

#include <pugixml.hpp>

namespace Zeyrixon
{
    BehaviorTree::BehaviorTree(const std::string& behaviorTreePath, World& currentWorld, uint64_t controlledEntityID)
        : m_ControlledEntityID(controlledEntityID), m_ActiveWorld(&currentWorld), m_BehaviorTreePath(behaviorTreePath)
    {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(behaviorTreePath.c_str());
        if (!result)
        {
            Z_CORE_ERROR("Failed to load BehaviorTree '{0}': {1}", behaviorTreePath, result.description());
            return;
        }

        pugi::xml_node root = doc.child("BehaviorTree");
        if (!root)
        {
            Z_CORE_ERROR("BehaviorTree file '{0}' has no <BehaviorTree> root node", behaviorTreePath);
            return;
        }

        if (!currentWorld.FindEntityByID(controlledEntityID))
        {
            Z_CORE_ERROR("BehaviorTree has no entity to control! Attempted ID is: {0}", controlledEntityID);
            return;
        }

        for (pugi::xml_node n : root.children("Node"))
        {
            BTNode node;
            node.ID = n.attribute("ID").as_int();
            node.Name = n.attribute("Name").as_string();
            node.Priority = n.attribute("Priority").as_int();

            if (n.attribute("Type").as_string() == "Selector")
                node.Type = NodeType::Selector;
            else if (n.attribute("Type").as_string() == "Sequence")
                node.Type = NodeType::Sequence;
            else if (n.attribute("Type").as_string() == "Task")
                node.Type = NodeType::Task;
            else
                node.Type = NodeType::Root;

            m_AllNodes.push_back(node);
        }

        m_BehaviorTreeName = root.attribute("Name").as_string("Untitled");
        m_IsValid = true;
    }

    BehaviorTree::~BehaviorTree()
    {
        m_ActiveWorld = nullptr;
    }

    void BehaviorTree::BTUpdate(DeltaTime dt)
    {
        Z_CORE_TRACE("Behavior Tree {0} Updating with DeltaTime of: {1}", m_BehaviorTreeName, dt.GetSeconds());
    }

    void BehaviorTree::RunTask(BTNode nodeInformation, TypeInfo informationType)
    {
        Z_CORE_TRACE("Behavior Tree is Running: {0}", nodeInformation.Name.c_str());
    }
}
