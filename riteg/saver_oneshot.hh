#ifndef RITEG_SAVER_ONESHOT_HH
#define RITEG_SAVER_ONESHOT_HH 1
#pragma once

#include "riteg/saver.hh"

class Saver_OneShot final : public Saver {
public:
    virtual ~Saver_OneShot(void) = default;
    virtual void init(void) override;
    virtual void step(void) override;

private:
    std::filesystem::path m_path;
    bool m_is_done = false;
};

#endif /* RITEG_SAVER_ONESHOT_HH */
