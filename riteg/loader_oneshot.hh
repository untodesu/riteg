#pragma once

#include "riteg/loader.hh"

class Loader_OneShot final : public Loader {
public:
    virtual ~Loader_OneShot(void) override = default;
    virtual void init(void) override;
    virtual void step(void) override;

private:
    std::filesystem::path m_path;
    bool m_is_done;
};
