#pragma once

#include "riteg/loader.hh"

class Loader_Sprintf final : public Loader {
public:
    virtual ~Loader_Sprintf(void) = default;
    virtual void init(void) override;
    virtual void step(void) override;

private:
    std::string m_format;
    std::string m_buffer;
    std::size_t m_frame;
    std::size_t m_limit;
    bool m_is_done;
};
