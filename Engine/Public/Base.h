#pragma once

class Base
{
public:
    Base();
    ~Base();

    virtual void OnPlay() {};
    virtual void EachFrame() {};
    virtual void EachPhysicsFrame() {};
};