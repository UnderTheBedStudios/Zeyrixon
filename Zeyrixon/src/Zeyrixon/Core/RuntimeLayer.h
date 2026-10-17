#pragma once

#include <Zeyrixon/Core/Core.h>
#include <Zeyrixon/Core/Layer.h>
#include <Zeyrixon/Core/Project.h>
#include <Zeyrixon/World/World.h>

#include <memory>
#include <string>

namespace Zeyrixon
{
    /* The "shipped game" half of the engine: loads a project, starts its startup world (which
       compiles and starts the scripts) and every frame updates it and draws what the primary
       Camera sees. No editor involved. A game project's main.cpp pushes one of these. */
    class Z_API RuntimeLayer : public Layer
    {
    public:
        RuntimeLayer(const std::string& projectManifestPath);

        void OnAttach() override;
        void OnDetach() override;
        void OnUpdate(DeltaTime dt) override;

        World* GetWorld() { return m_World.get(); }

    private:
        std::string m_ManifestPath;
        std::shared_ptr<Project> m_Project;
        std::shared_ptr<World> m_World;
    };
}
