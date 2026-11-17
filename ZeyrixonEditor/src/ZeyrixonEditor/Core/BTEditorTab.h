#pragma once

#include <GraphEditor.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <pugixml.hpp>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace Editor
{
    /* The data behind one open .zbeht file, plus the callbacks GraphEditor::Show needs.
       GraphEditor's naming is backwards from what you'd guess: a link's "input node" is where the line
       STARTS (it uses that node's output slot), so for a behavior tree that is the PARENT, and the
       link's "output node" is the CHILD. */
    class BTGraphDelegate : public GraphEditor::Delegate
    {
    public:
        enum NodeType : GraphEditor::TemplateIndex
        {
            Root = 0,
            Selector = 1,
            Sequence = 2,
            Task = 3
        };

        struct BTNode
        {
            std::string Name;
            NodeType Type = Task;
            float X = 0.0f;
            float Y = 0.0f;
            bool Selected = false;
        };

        std::vector<BTNode> Nodes;
        std::vector<GraphEditor::Link> Links;
        std::string TreeName = "NewBT";
        bool Dirty = false;

        // Filled by RightClick (called from inside GraphEditor::Show), consumed by the tab afterwards.
        bool ContextMenuRequested = false;
        GraphEditor::NodeIndex ContextMenuNode = static_cast<GraphEditor::NodeIndex>(-1);

        BTGraphDelegate()
        {
            static const char* parentNames[]   = { "Parent" };
            static const char* childrenNames[] = { "Children" };
            static ImU32 slotColors[]          = { IM_COL32(200, 200, 200, 255) };

            auto make = [&](ImU32 header, ImU8 inputs, ImU8 outputs)
            {
                GraphEditor::Template t{};
                t.mHeaderColor = header;
                t.mBackgroundColor = IM_COL32(40, 40, 40, 200);
                t.mBackgroundColorOver = IM_COL32(60, 60, 60, 200);
                t.mInputCount = inputs;
                t.mInputNames = parentNames;
                t.mInputColors = slotColors;
                t.mOutputCount = outputs;
                t.mOutputNames = childrenNames;
                t.mOutputColors = slotColors;
                return t;
            };

            m_Templates[Root]       = make(IM_COL32(170, 60, 60, 255), 0, 1);
            m_Templates[Selector]   = make(IM_COL32(60, 110, 170, 255), 1, 1);
            m_Templates[Sequence]   = make(IM_COL32(160, 200, 250, 255), 1, 1);
            m_Templates[Task]       = make(IM_COL32(60, 150, 90, 255), 1, 0);
        }

        // ---- file io -------------------------------------------------------------------------

        static const char* TypeToString(NodeType type)
        {
            switch (type)
            {
                case Root:      return "Root";
                case Selector:  return "Selector";
                case Sequence:  return "Sequence";
                default:        return "Task";
            }
        }

        static NodeType TypeFromString(const std::string& s)
        {
            if (s == "Root")        return Root;
            if (s == "Selector")    return Selector;
            if (s == "Sequence")    return Sequence;
            return Task;
        }

        /* PLACEHOLDER SCHEMA - swap this for whatever your behavior tree runtime reads:
             <BehaviorTree Name="NewBT">
               <Node Type="Root" Name="Root" X="0" Y="0"/>
               <Link Parent="0" Child="1"/>        (indices into the Node list, in order)
             </BehaviorTree>
           An empty tree (what "Create Behavior Tree" writes) loads as just a Root node. */
        bool Load(const std::filesystem::path& path)
        {
            pugi::xml_document doc;
            if (!doc.load_file(path.string().c_str()))
                return false;

            pugi::xml_node root = doc.child("BehaviorTree");
            if (!root)
                return false;

            Nodes.clear();
            Links.clear();
            TreeName = root.attribute("Name").as_string("NewBT");

            for (pugi::xml_node n : root.children("Node"))
            {
                BTNode node;
                node.Name = n.attribute("Name").as_string("Node");
                node.Type = TypeFromString(n.attribute("Type").as_string());
                node.X = n.attribute("X").as_float();
                node.Y = n.attribute("Y").as_float();
                Nodes.push_back(std::move(node));
            }

            for (pugi::xml_node l : root.children("Link"))
            {
                const size_t parent = l.attribute("Parent").as_ullong(static_cast<unsigned long long>(-1));
                const size_t child = l.attribute("Child").as_ullong(static_cast<unsigned long long>(-1));
                if (parent >= Nodes.size() || child >= Nodes.size() || parent == child)
                    continue;

                GraphEditor::Link link{};
                link.mInputNodeIndex = parent;
                link.mInputSlotIndex = 0;
                link.mOutputNodeIndex = child;
                link.mOutputSlotIndex = 0;
                Links.push_back(link);
            }

            if (Nodes.empty())
                Nodes.push_back({ "Root", Root, 0.0f, 0.0f, false });

            Dirty = false;
            return true;
        }

        bool Save(const std::filesystem::path& path)
        {
            pugi::xml_document doc;
            pugi::xml_node decl = doc.append_child(pugi::node_declaration);
            decl.append_attribute("version") = "1.0";

            pugi::xml_node root = doc.append_child("BehaviorTree");
            root.append_attribute("Name") = TreeName.c_str();

            for (const BTNode& n : Nodes)
            {
                pugi::xml_node node = root.append_child("Node");
                node.append_attribute("Type") = TypeToString(n.Type);
                node.append_attribute("Name") = n.Name.c_str();
                node.append_attribute("X") = n.X;
                node.append_attribute("Y") = n.Y;
            }

            for (const GraphEditor::Link& l : Links)
            {
                pugi::xml_node link = root.append_child("Link");
                link.append_attribute("Parent") = static_cast<unsigned long long>(l.mInputNodeIndex);
                link.append_attribute("Child") = static_cast<unsigned long long>(l.mOutputNodeIndex);
            }

            if (!doc.save_file(path.string().c_str()))
                return false;

            Dirty = false;
            return true;
        }

        // ---- editing -------------------------------------------------------------------------

        void AddNode(const char* name, NodeType type, ImVec2 position)
        {
            Nodes.push_back({ name, type, position.x, position.y, false });
            Dirty = true;
        }

        void DeleteNode(size_t index)
        {
            if (index >= Nodes.size() || Nodes[index].Type == Root)
                return;

            Nodes.erase(Nodes.begin() + static_cast<std::ptrdiff_t>(index));
            Links.erase(std::remove_if(Links.begin(), Links.end(), [&](const GraphEditor::Link& l)
            {
                return l.mInputNodeIndex == index || l.mOutputNodeIndex == index;
            }), Links.end());

            // Everything after the deleted node moved down one slot.
            for (GraphEditor::Link& l : Links)
            {
                if (l.mInputNodeIndex > index)  --l.mInputNodeIndex;
                if (l.mOutputNodeIndex > index) --l.mOutputNodeIndex;
            }
            Dirty = true;
        }

        // ---- GraphEditor::Delegate -----------------------------------------------------------

        bool AllowedLink(GraphEditor::NodeIndex from, GraphEditor::NodeIndex to) override
        {
            return from != to;
        }

        void SelectNode(GraphEditor::NodeIndex nodeIndex, bool selected) override
        {
            if (nodeIndex < Nodes.size())
                Nodes[nodeIndex].Selected = selected;
        }

        void MoveSelectedNodes(const ImVec2 delta) override
        {
            for (BTNode& n : Nodes)
            {
                if (n.Selected)
                {
                    n.X += delta.x;
                    n.Y += delta.y;
                }
            }
            Dirty = true;
        }

        void RightClick(GraphEditor::NodeIndex nodeIndex, GraphEditor::SlotIndex, GraphEditor::SlotIndex) override
        {
            ContextMenuRequested = true;
            ContextMenuNode = nodeIndex;   // -1 when the click landed on empty canvas
        }

        void AddLink(GraphEditor::NodeIndex inputNodeIndex, GraphEditor::SlotIndex inputSlotIndex,
                     GraphEditor::NodeIndex outputNodeIndex, GraphEditor::SlotIndex outputSlotIndex) override
        {
            GraphEditor::Link link{};
            link.mInputNodeIndex = inputNodeIndex;
            link.mInputSlotIndex = inputSlotIndex;
            link.mOutputNodeIndex = outputNodeIndex;
            link.mOutputSlotIndex = outputSlotIndex;
            Links.push_back(link);
            Dirty = true;
        }

        void DelLink(GraphEditor::LinkIndex linkIndex) override
        {
            if (linkIndex < Links.size())
            {
                Links.erase(Links.begin() + static_cast<std::ptrdiff_t>(linkIndex));
                Dirty = true;
            }
        }

        void CustomDraw(ImDrawList*, ImRect, GraphEditor::NodeIndex) override {}

        const size_t GetTemplateCount() override { return 4; }
        const GraphEditor::Template GetTemplate(GraphEditor::TemplateIndex index) override { return m_Templates[index]; }

        const size_t GetNodeCount() override { return Nodes.size(); }
        const GraphEditor::Node GetNode(GraphEditor::NodeIndex index) override
        {
            const BTNode& n = Nodes[index];
            GraphEditor::Node node{};
            node.mName = n.Name.c_str();
            node.mTemplateIndex = n.Type;
            node.mRect = ImRect(ImVec2(n.X, n.Y), ImVec2(n.X + 200.0f, n.Y + 100.0f));
            node.mSelected = n.Selected;
            return node;
        }

        const size_t GetLinkCount() override { return Links.size(); }
        const GraphEditor::Link GetLink(GraphEditor::LinkIndex index) override { return Links[index]; }

    private:
        GraphEditor::Template m_Templates[4]{};
    };

    // One tab in the editor's tab bar.
    struct BTEditorTab
    {
        std::filesystem::path Path;
        std::string Label;            // "file.zbeht##/full/path" - the ## part keeps the tab ID unique
        std::string Status;
        bool Open = true;             // ImGui clears this when the tab's X is clicked
        bool RequestFocus = false;    // set when the file is double-clicked again while already open

        BTGraphDelegate Graph;
        GraphEditor::Options Options;
        GraphEditor::ViewState View;
        GraphEditor::FitOnScreen Fit = GraphEditor::Fit_AllNodes;   // frame the whole tree the first time

        bool Dirty() const { return Graph.Dirty; }

        bool Load(const std::filesystem::path& path)
        {
            if (!Graph.Load(path))
                return false;

            Path = path;
            Label = path.filename().string() + "##" + path.string();
            return true;
        }

        bool Save()
        {
            const bool ok = Graph.Save(Path);
            Status = ok ? "Saved" : "Save failed";
            return ok;
        }

        // Call between BeginTabItem and EndTabItem.
        void Draw()
        {
            if (ImGui::Button("Save"))
                Save();
            ImGui::SameLine();
            if (ImGui::Button("Fit View"))
                Fit = GraphEditor::Fit_AllNodes;
            ImGui::SameLine();
            ImGui::TextDisabled("%s", Status.empty() ? Path.string().c_str() : Status.c_str());

            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)
                && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false))
                Save();

            ImGui::Separator();

            if (ImGui::BeginChild("##GraphCanvas", ImVec2(0.0f, 0.0f), false,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
            {
                const ImVec2 canvasMin = ImGui::GetCursorScreenPos();
                GraphEditor::Show(Graph, Options, View, true, &Fit);
                DrawContextMenu(canvasMin);
            }
            ImGui::EndChild();
        }

    private:
        GraphEditor::NodeIndex m_MenuNode = static_cast<GraphEditor::NodeIndex>(-1);
        ImVec2 m_MenuGraphPos{ 0.0f, 0.0f };

        void DrawContextMenu(const ImVec2& canvasMin)
        {
            if (Graph.ContextMenuRequested)
            {
                Graph.ContextMenuRequested = false;
                m_MenuNode = Graph.ContextMenuNode;

                // Inverse of how GraphEditor places nodes: screen = canvasMin + (graph + scroll) * zoom.
                const ImVec2 mouse = ImGui::GetMousePos();
                m_MenuGraphPos = ImVec2((mouse.x - canvasMin.x) / View.mFactor - View.mPosition.x,
                                        (mouse.y - canvasMin.y) / View.mFactor - View.mPosition.y);
                ImGui::OpenPopup("##BTContext");
            }

            if (ImGui::BeginPopup("##BTContext"))
            {
                if (m_MenuNode < Graph.Nodes.size())
                {
                    const bool isRoot = Graph.Nodes[m_MenuNode].Type == BTGraphDelegate::Root;
                    if (ImGui::MenuItem("Delete Node", nullptr, false, !isRoot))
                        Graph.DeleteNode(m_MenuNode);
                }
                else
                {
                    if (ImGui::MenuItem("Add Selector"))
                        Graph.AddNode("Selector", BTGraphDelegate::Selector, m_MenuGraphPos);
                    if (ImGui::MenuItem("Add Sequence"))
                        Graph.AddNode("Sequence", BTGraphDelegate::Sequence, m_MenuGraphPos);
                    if (ImGui::MenuItem("Add Task"))
                        Graph.AddNode("Task", BTGraphDelegate::Task, m_MenuGraphPos);
                }
                ImGui::EndPopup();
            }
        }
    };
}