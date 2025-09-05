#pragma once

class Source;

class Saver {
public:
    static bool write_source_RGBA(const Source* source, const std::filesystem::path& path);

    virtual ~Saver(void) = default;
    virtual void init(void) = 0;
    virtual void step(void) = 0;
};
