#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/DeltaTime.h>
#include <Zeyrixon/World/World.h>

#include <string>
#include <vector>

namespace Zeyrixon
{
    struct TypeInfo
    {
        const char* Name;
        uint32_t Header;
        int Inputs;
        int Outputs;
    };

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

        void RunTask(BTNode nodeInformation, TypeInfo informationType);

        void BTUpdate(DeltaTime dt);

    private:
        uint64_t m_ControlledEntityID;
        World* m_ActiveWorld = nullptr;
        bool m_IsValid = false;
        std::string m_BehaviorTreePath = "";
        std::string m_BehaviorTreeName = "BT";
        int m_LastNodeIndex = -1;

        std::vector<BTNode> m_AllNodes;
    };
}
