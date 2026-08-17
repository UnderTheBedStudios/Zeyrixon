#pragma once

class Base
{
public:
    Base();
    virtual ~Base();

    virtual void OnPlay() {};
    virtual void EachFrame() {};
    virtual void EachPhysicsFrame() {};
};