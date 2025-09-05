#pragma once

#include "riteg/saver.hh"

class Saver_Sprintf final : public Saver {
public:
    virtual ~Saver_Sprintf(void) override = default;
    virtual void init(void) override;
    virtual void step(void) override;

private:
    std::string m_format;
    std::string m_buffer;
    std::size_t m_frame;
    std::size_t m_limit;
    bool m_is_done;
};
