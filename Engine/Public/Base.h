#pragma once

#include <string>

class Base
{
public:
    Base();
    virtual ~Base();

	std::string GetName() const { return m_Name; }
	std::string GetType() const { return m_Type; }

    virtual void OnPlay() {};
    virtual void EachFrame() {};
    virtual void EachPhysicsFrame() {};

private:
	std::string m_Name = "";
	std::string m_Type = "";
};
