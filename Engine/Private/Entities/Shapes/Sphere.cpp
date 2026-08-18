#include <Engine/Public/Entities/Shapes/Sphere.h>
#include <string>
#include <memory>

Sphere::Sphere(std::string assetRoot)
    : PrimativeShape(assetRoot, "/Engine/Models/Sphere/sphere.obj")
{
}

Sphere::~Sphere()
{
}