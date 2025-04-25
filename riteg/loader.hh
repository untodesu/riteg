#ifndef RITEG_LOADER_HH
#define RITEG_LOADER_HH 1
#pragma once

class Loader {
public:
    virtual ~Loader(void) = default;
    virtual void init(void) = 0;
    virtual void step(void) = 0;
};

#endif /* RITEG_LOADER_HH */
