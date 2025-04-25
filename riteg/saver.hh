#ifndef RITEG_SAVER_HH
#define RITEG_SAVER_HH 1
#pragma once

class Source;

class Saver {
public:
    virtual ~Saver(void) = default;
    virtual void init(void) = 0;
    virtual void step(void) = 0;

public:
    static bool write_source_RGBA(const Source *source, const std::filesystem::path &path);
};

#endif /* RITEG_SAVER_HH */
