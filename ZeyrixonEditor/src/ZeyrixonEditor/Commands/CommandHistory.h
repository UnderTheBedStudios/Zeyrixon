#pragma once

#include <ZeyrixonEditor/Commands/Command.h>
#include <vector>

class CommandHistory
{
public:
    void Do(std::unique_ptr<Command> cmd)
    {
        cmd->Execute();
        Record(std::move(cmd));
    }

    /* Record a command whos effect has already been applied
       (useful for gizmo drags where the change happened live). */
    void Record(std::unique_ptr<Command> cmd)
    {
        m_Redo.clear(); // a new action invalidates the redo branch

        if (!m_Undo.empty() && m_Undo.back()->MergeWith(*cmd))
            return;

        m_Undo.push_back(std::move(cmd));
        if (m_Undo.size() > m_MaxDepth)
            m_Undo.erase(m_Undo.begin());
    }

    void Undo()
    {
        if (m_Undo.empty()) return;
        auto cmd = std::move(m_Undo.back());
        m_Undo.pop_back();
        cmd->Undo();
        m_Redo.push_back(std::move(cmd));
    }

    void Redo()
    {
        if (m_Redo.empty()) return;
        auto cmd = std::move(m_Redo.back());
        m_Redo.pop_back();
        cmd->Execute();
        m_Undo.push_back(std::move(cmd));
    }

    bool CanUndo() const { return !m_Undo.empty(); }
    bool CanRedo() const { return !m_Redo.empty(); }
    void Clear() { m_Undo.clear(); m_Redo.clear(); }

private:
    std::vector<std::unique_ptr<Command>> m_Undo, m_Redo;
    size_t m_MaxDepth = 100;
};