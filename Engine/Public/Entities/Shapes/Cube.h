#pragma once

#include <Engine/Public/Entities/Shapes/PrimativeShape.h>

class Cube : public PrimativeShape
{
public:
    Cube(std::string assetRoot);
    ~Cube();
};