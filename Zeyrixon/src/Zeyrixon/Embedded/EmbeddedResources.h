#pragma once

/* Resources compiled into the binary (see tools/EmbedResources.py), so builds run standalone
   without any loose files next to the executable. */

namespace Zeyrixon::Embedded
{
    /* JetBrainsMono Nerd Font, ImGui-compressed: pass to ImGui's AddFontFromMemoryCompressedTTF. */
    extern const unsigned int NerdFont_compressed_size;
    extern const unsigned char NerdFont_compressed_data[];

    /* Raw PNG file bytes: pass to OpenGLTexture2D / Application::SetWindowImageFromMemory. */
    extern const unsigned char WindowIconPng[];
    extern const unsigned long long WindowIconPngSize;
    extern const unsigned char FolderClosedPng[];
    extern const unsigned long long FolderClosedPngSize;
    extern const unsigned char FolderOpenPng[];
    extern const unsigned long long FolderOpenPngSize;
    extern const unsigned char FilePng[];
    extern const unsigned long long FilePngSize;
    extern const unsigned char DirectionalLightPng[];
    extern const unsigned long long DirectionalLightPngSize;
    extern const unsigned char PointLightPng[];
    extern const unsigned long long PointLightPngSize;
    extern const unsigned char SpotLightPng[];
    extern const unsigned long long SpotLightPngSize;
}
