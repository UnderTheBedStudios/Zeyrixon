#include <Editor/Public/Windows/Gui/BaseGui.h>
#include <imgui.h>

BaseGui::BaseGui()
{

}

BaseGui::~BaseGui() = default;

void BaseGui::DrawGui()
{
    ImGui::Text("Hi");
}