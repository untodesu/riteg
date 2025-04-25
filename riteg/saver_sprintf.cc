#include "riteg/pch.hh"
#include "riteg/saver_sprintf.hh"

#include "riteg/cmdline.hh"
#include "riteg/project.hh"

void Saver_Sprintf::init(void)
{
    auto sprintf_format = cmdline::get("oformat");
    auto sprintf_limit = cmdline::get("omaxframes");

    riteg_force_assert_msg(sprintf_format, "Invalid argument [oformat]");

    m_format = sprintf_format;
    m_limit = sprintf_limit ? std::stoul(sprintf_limit) : SIZE_MAX;

    riteg_force_assert_msg(m_limit > 0, "Invalid argument [omaxframes]");

    m_frame = 1;
    m_is_done = false;
    m_buffer.clear();
}

void Saver_Sprintf::step(void)
{
    if(!m_is_done && m_frame <= m_limit) {
        m_buffer.resize(2 + std::snprintf(nullptr, 0, m_format.c_str(), static_cast<int>(m_frame)));
        std::snprintf(m_buffer.data(), m_buffer.size(), m_format.c_str(), static_cast<int>(m_frame));

        if(!Saver::write_source_RGBA(project::get_output_source(), m_buffer)) {
            riteg_warning << "failed to write image: " << m_buffer << std::endl;
            m_is_done = true;
            return;
        }

        riteg_info << "wrote image: " << m_buffer << std::endl;

        m_frame += 1;
    }
}
