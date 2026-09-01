#include <pch.h>
#include <Zeyrixon/Layer.h>

namespace Zeyrixon
{
    Layer::Layer(const std::string& debugName)
        : m_DebugName(debugName)
    {}

    Layer::~Layer()
    {}
}