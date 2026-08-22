#pragma once

#include "Engine/Public/Base.h"
#include <Engine/Public/Common/Reflection.h>

class BaseComponent : public Base
{
public:
	BaseComponent();
	virtual ~BaseComponent();

	virtual const std::vector<FieldInfo>* GetReflectedFields() const { return nullptr; }

	virtual const char* GetComponentTypeName() const { return "Component"; }
};
