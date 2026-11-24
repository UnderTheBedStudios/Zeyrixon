#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/DeltaTime.h>
#include <Zeyrixon/World/World.h>

#include <string>
#include <cstddef>
#include <vector>

namespace Zeyrixon
{
    enum NodeType : uint32_t
    {
        Root = 0,
        Selector = 1,
        Sequence = 2,
        Task = 3,

        TemplateCount
    };

    struct BTNode
    {
        std::string Name;
        NodeType Type = Task;
        float X = 0.0f;
        float Y = 0.0f;
        bool Selected = false;
        int ID = 0;
        int Priority = 0;
    };

    class BehaviorTree
    {
    public:
        BehaviorTree(const std::string& behaviorTreePath, World& currentWorld, uint64_t controlledEntityID);
        ~BehaviorTree();

        bool IsValid() const { return m_IsValid; }

        void RunTask(BTNode nodeInformation);

        void BTUpdate(DeltaTime dt);

    private:
        void TickNode(size_t nodeIndex, std::vector<bool>& visited);

        uint64_t m_ControlledEntityID;
        World* m_ActiveWorld = nullptr;
        bool m_IsValid = false;
        std::string m_BehaviorTreePath = "";
        std::string m_BehaviorTreeName = "BT";
        std::vector<BTNode> m_AllNodes;
        // The editor serializes links using node indices, not node IDs.
        std::vector<std::vector<size_t>> m_Children;
    };
}
