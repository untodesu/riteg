#include "riteg/pch.hh"

#include "riteg/saver.hh"

#include "riteg/source.hh"

bool Saver::write_source_RGBA(const Source* source, const std::filesystem::path& path)
{
    static std::vector<std::byte> s_cache;

    assert(source != nullptr);

    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), [](char character) {
        return std::tolower(character);
    });

    auto width = source->get_texture_width();
    auto height = source->get_texture_height();
    auto framebuffer = source->get_framebuffer();

    assert(framebuffer != 0);

    s_cache.resize(width * height * 4);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuffer);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, s_cache.data());

    stbi_flip_vertically_on_write(true);

    if(0 == std::strcmp(extension.c_str(), ".png"))
        return stbi_write_png(path.string().c_str(), width, height, 4, s_cache.data(), width * 4);
    if(0 == std::strcmp(extension.c_str(), ".jpg") || !std::strcmp(extension.c_str(), ".jpeg"))
        return stbi_write_jpg(path.string().c_str(), width, height, 4, s_cache.data(), 100);
    if(0 == std::strcmp(extension.c_str(), ".bmp"))
        return stbi_write_bmp(path.string().c_str(), width, height, 4, s_cache.data());
    if(0 == std::strcmp(extension.c_str(), ".tga"))
        return stbi_write_tga(path.string().c_str(), width, height, 4, s_cache.data());
    return false;
}
