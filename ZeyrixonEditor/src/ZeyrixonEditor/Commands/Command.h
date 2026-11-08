#pragma once

#include <memory>
#include <string>

class Command
{
public:
    virtual ~Command() = default;
    virtual void Execute() = 0;
    virtual void Undo() = 0;
    virtual const char* GetName() const = 0;

    // Lets rapid edits (e.g. dragging a slider) collapse into one undo step.
    virtual bool MergeWith(const Command& other) { return false; }
};