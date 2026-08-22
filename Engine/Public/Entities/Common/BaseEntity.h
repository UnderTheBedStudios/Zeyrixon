#pragma once

#include "Engine/Public/Base.h"
#include <Engine/Public/Components/Common/BaseComponent.h>
#include <string>
#include <type_traits>
#include <vector>

class BaseEntity : public Base
{
public:
	BaseEntity();
	~BaseEntity() override = default;

	std::string Name;

	[[nodiscard]] std::string GetName() const { return Name; }
	[[nodiscard]] virtual const char* GetTypeName() const { return "Entity"; }

	[[nodiscard]] std::vector<BaseComponent*> GetComponents() const { return m_Components; }

	template <typename T>
	T* AddComponent()
	{
		static_assert(std::is_base_of_v<BaseComponent, T>,
						  "Error: The specified class type must inherit from BaseComponent!");

		T* newComponent = new T;
		m_Components.push_back(newComponent);
		return newComponent;
	}

	[[nodiscard]] BaseComponent* GetComponent(std::string name, std::string type);

private:
	std::vector<BaseComponent*> m_Components;
};
