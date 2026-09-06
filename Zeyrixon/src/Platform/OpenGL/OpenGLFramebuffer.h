#pragma once

#include <Zeyrixon/Core/Core.h>
#include <cstdint>

namespace Zeyrixon
{
    struct FramebufferSpecification
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t Samples = 1;
    };

    class Z_API OpenGLFramebuffer
    {
    public:
        OpenGLFramebuffer(const FramebufferSpecification& spec);
        ~OpenGLFramebuffer();

        void Bind();
        void Unbind();

        void Resize(uint32_t width, uint32_t height);

        uint32_t GetColorAttachmentRenderID() const { return m_ColorAttachment; }
        const FramebufferSpecification& GetSpecifacation() const { return m_Specifacation; }
        
    private:
        void Invalidate();

        uint32_t m_RendererID = 0;
        uint32_t m_ColorAttachment = 0;
        uint32_t m_DepthAttachment = 0;

        FramebufferSpecification m_Specifacation;
    };
}