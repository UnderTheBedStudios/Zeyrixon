#include <Engine/Public/Entities/Shapes/Cube.h>
#include <string>
#include <memory>

Cube::Cube(std::string assetRoot)
    : PrimativeShape(assetRoot, "/Engine/Models/Cube/cube.obj")
{
}

Cube::~Cube()
{
}