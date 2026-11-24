#include <Zeyrixon/World/BehaviorTree.h>
#include <Zeyrixon/Core/Log.h>
#include <Zeyrixon/World/Entity.h>

#include <pugixml.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

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
        m_BehaviorTreeName = root.attribute("Name").as_string("Untitled");

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
        m_Children.resize(m_AllNodes.size());
        for (pugi::xml_node link : root.children("Link"))
        {
            // BTEditorTab writes Parent and Child as positions in the Node list.
            const size_t parent = static_cast<size_t>(link.attribute("Parent").as_ullong(static_cast<unsigned long long>(-1)));
            const size_t child = static_cast<size_t>(link.attribute("Child").as_ullong(static_cast<unsigned long long>(-1)));
            if (parent >= m_AllNodes.size() || child >= m_AllNodes.size() || parent == child)
            {
                Z_CORE_WARN("BehaviorTree '{0}' contains an invalid link ({1} -> {2})", m_BehaviorTreeName, parent, child);
                continue;
            }
            m_Children[parent].push_back(child);
        }

        m_IsValid = true;
    }

    BehaviorTree::~BehaviorTree()
    {
        m_ActiveWorld = nullptr;
    }

    void BehaviorTree::BTUpdate(DeltaTime dt)
    {
        Z_CORE_TRACE("Behavior Tree {0} Updating with DeltaTime of: {1}", m_BehaviorTreeName, dt.GetSeconds());
        if (m_AllNodes.empty())
        {
            Z_CORE_WARN("BehaviorTree '{0}' has no nodes", m_BehaviorTreeName);
            return;
        }

        // Start at the explicit Root node when present; older files may not have one.
        size_t rootIndex = 0;
        for (size_t i = 0; i < m_AllNodes.size(); ++i)
        {
            if (m_AllNodes[i].Type == NodeType::Root)
            {
                rootIndex = i;
                break;
            }
        }

        std::vector<bool> visited(m_AllNodes.size(), false);
        TickNode(rootIndex, visited);
    }

    void BehaviorTree::RunTask(BTNode nodeInformation)
    {
        Z_CORE_TRACE("Behavior Tree '{0}' is running node '{1}' (ID {2})",
                     m_BehaviorTreeName, nodeInformation.Name, nodeInformation.ID);
    }

    void BehaviorTree::TickNode(size_t nodeIndex, std::vector<bool>& visited)
    {
        if (nodeIndex >= m_AllNodes.size() || visited[nodeIndex])
            return;

        visited[nodeIndex] = true;
        const BTNode& node = m_AllNodes[nodeIndex];
        RunTask(node);

        // Lower numbered priorities run first. Zero means no saved priority and
        // sorts after explicitly prioritized siblings; stable_sort keeps link
        // order for ties.
        std::vector<size_t> children = m_Children[nodeIndex];
        std::stable_sort(children.begin(), children.end(), [this](size_t left, size_t right)
        {
            const int leftPriority = m_AllNodes[left].Priority == 0
                ? std::numeric_limits<int>::max() : m_AllNodes[left].Priority;
            const int rightPriority = m_AllNodes[right].Priority == 0
                ? std::numeric_limits<int>::max() : m_AllNodes[right].Priority;
            return leftPriority < rightPriority;
        });
        // Until task nodes report Success/Failure/Running, composites dispatch
        // their connected children in priority order.
        for (size_t childIndex : children)
            TickNode(childIndex, visited);
    }
}
