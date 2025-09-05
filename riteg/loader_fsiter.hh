#pragma once

#include "riteg/loader.hh"

class Loader_FSIter final : public Loader {
public:
    virtual ~Loader_FSIter(void) override = default;
    virtual void init(void) override;
    virtual void step(void) override;

private:
    std::queue<std::filesystem::path> m_paths;
};
