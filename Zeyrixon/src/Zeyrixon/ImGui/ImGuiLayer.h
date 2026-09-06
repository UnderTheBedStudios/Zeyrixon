#pragma once

#include <cstdint>

#include <Zeyrixon/Core/Layer.h>

#include <Zeyrixon/Events/ApplicationEvent.h>
#include <Zeyrixon/Events/KeyEvent.h>
#include <Zeyrixon/Events/MouseEvent.h>


namespace Zeyrixon
{
    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer() = default;

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnEvent(Event& e) override;

        void Begin();
        void End();

        void BlockEvents(bool block) { m_BlockEvents = block; }

        void SetDarkThemeColors();

        uint32_t GetActiveWidgetID() const;
    private:
        bool m_BlockEvents = true;
    };
}