#pragma once

class Loader {
public:
    virtual ~Loader(void) = default;
    virtual void init(void) = 0;
    virtual void step(void) = 0;
};
