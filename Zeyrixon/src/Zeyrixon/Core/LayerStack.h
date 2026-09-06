#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/Layer.h>

#include <vector>

namespace Zeyrixon
{
    class Z_API LayerStack
    {
    public:
        LayerStack();
        ~LayerStack();

        void PushLayer(Layer* layer);
        void PushOverlay(Layer* overlay);
        void PopLayer(Layer* layer);
        void PopOverlay(Layer* layer);

        std::vector<Layer*>::iterator begin() { return m_Layers.begin(); }
        std::vector<Layer*>::iterator end()   { return m_Layers.end(); }
    private:
        std::vector<Layer*> m_Layers;
        unsigned int m_LayerInsertIndex = 0;
        std::vector<Layer*>::iterator m_LayerInsert;
    };
}