#include <Engine/Public/Entities/Shapes/PrimativeShape.h>
#include <string>

PrimativeShape::PrimativeShape(std::string assetRoot, std::string shapePath)
{
	m_BaseModel = AddComponent<Model>();

    std::string shapeModelPath = assetRoot + shapePath;
    if (!m_BaseModel->LoadFromFile(shapeModelPath))
        fprintf(stderr, "[Engine] Failed to load Camera model from %s\n", shapeModelPath.c_str());
}
PrimativeShape::~PrimativeShape()
{

}
