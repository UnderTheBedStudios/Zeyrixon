#pragma once

#include <Engine/Public/Entities/Shapes/PrimativeShape.h>

class Sphere : public PrimativeShape
{
public:
    Sphere(std::string assetRoot);
    ~Sphere();
};